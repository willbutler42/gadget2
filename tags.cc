#include "tags.h"
#include "areatime.h"
#include "errorhandler.h"
#include "readfunc.h"
#include "readword.h"
#include "stock.h"
#include "stockprey.h"
#include "gadget.h"

Tags::Tags(CommentStream& infile, const char* givenname, const AreaClass* const Area,
  const TimeClass* const TimeInfo, Keeper* const keeper, StockPtrVector stockvec)
  : HasName(givenname) {

  LgrpDiv = NULL;
  taggingstock = 0;
  ErrorHandler handle;
  char text[MaxStrLength];
  strncpy(text, "", MaxStrLength);
  ifstream subfile;
  CommentStream subcomment(subfile);

  keeper->AddString("tags");
  keeper->AddString(givenname);

  //Currently can only have one stock per tagging experiment
  readWordAndValue(infile, "stock", text);
  stocknames.resize(1);
  stocknames[0] = new char[strlen(text) + 1];
  strcpy(stocknames[0], text);

  int tmparea;
  //Currently can only have one area per tagging experiment
  readWordAndVariable(infile, "tagarea", tmparea);
  if ((tagarea = Area->InnerArea(tmparea)) == -1)
    handle.UndefinedArea(tmparea);

  infile >> ws;
  char c = infile.peek();
  if ((c == 'e') || (c == 'E'))
    readWordAndVariable(infile, "endyear", endyear);
  else
    endyear = TimeInfo->LastYear();

  int i = 0, found = 0;
  while (found == 0 && i < stockvec.Size()) {
    if (strcasecmp(stockvec[i]->Name(), stocknames[0]) == 0) {
      LgrpDiv = new LengthGroupDivision(*(stockvec[i]->ReturnLengthGroupDiv()));
      found++;
    }
    i++;
  }
  if (found == 0) {
    cerr << "Error when searching for names of stocks for tags\n"
      << "Did not find any name matching " << stocknames[0] << endl;
    exit(EXIT_FAILURE);
  }
  NumberByLength.resize(LgrpDiv->NoLengthGroups(), 0.0);

  //Now read in the tagloss information
  readWordAndFormula(infile, "tagloss", tagloss);
  tagloss.Inform(keeper);

  //Read in the numbers format: tagid - length - number
  readWordAndValue(infile, "numbers", text);
  subfile.open(text, ios::in);
  checkIfFailure(subfile, text);
  handle.Open(text);
  ReadNumbers(subcomment, givenname, TimeInfo);
  handle.Close();
  subfile.close();
  subfile.clear();
  keeper->ClearLast();
  keeper->ClearLast();
}

void Tags::ReadNumbers(CommentStream& infile, const char* tagname, const TimeClass* const TimeInfo) {

  int year, step;
  int i, lenid, keepdata, timeid;
  double tmplength, tmpnumber;
  char tmpname[MaxStrLength];
  strncpy(tmpname, "", MaxStrLength);
  ErrorHandler handle;

  infile >> ws;
  //Check the number of columns in the inputfile
  if (countColumns(infile) != 5)
    handle.Message("Wrong number of columns in inputfile - should be 5");

  while (!infile.eof()) {
    keepdata = 0;
    infile >> tmpname >> year >> step >> tmplength >> tmpnumber >> ws;

    //only keep the data if tmpname matches tagname
    if (!(strcasecmp(tagname, tmpname) == 0))
      keepdata = 1;

    //only keep the data if the length is valid
    lenid = -1;
    for (i = 0; i < LgrpDiv->NoLengthGroups(); i++)
      if (tmplength == LgrpDiv->Minlength(i))
        lenid = i;

    if (lenid == -1)
      keepdata = 1;

    //only keep the data if the number is positive
    if (tmpnumber < 0) {
      cerr << "Warning - found a negative number for the tagging experiment - number set to zero\n";
      keepdata = 1;
    }

    timeid = -1;
    if ((TimeInfo->IsWithinPeriod(year, step)) && (keepdata == 0)) {
      for (i = 0; i < Years.Size(); i++)
        if ((Years[i] == year) && (Steps[i] == step))
          timeid = i;

      if (timeid == -1) {
        Years.resize(1, year);
        Steps.resize(1, step);
        timeid = Years.Size() - 1;
        NumberByLengthMulti.resize(1, new DoubleMatrix(1, LgrpDiv->NoLengthGroups(), 0.0));
      }

    } else
      keepdata = 1;

    if (keepdata == 0)
      (*NumberByLengthMulti[timeid])[0][lenid] = tmpnumber;
  }

  tagyear = 9999;
  tagstep = 9999;
  for (i = 0; i < Years.Size(); i++)
    if ((Years[i] < tagyear) || (Years[i] == tagyear && Steps[i] < tagstep)) {
      tagyear = Years[i];
      tagstep = Steps[i];
    }

  timeid = -1;
  for (i = 0; i < Years.Size(); i++)
    if ((Years[i] == tagyear) && (Steps[i] == tagstep))
      timeid = i;

  if (timeid == -1) {
    cerr << "Error in tagging experiment - invalid year " << tagyear << " and step " << tagstep << endl;
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < LgrpDiv->NoLengthGroups(); i++)
    NumberByLength[i] = (*NumberByLengthMulti[timeid])[0][i];
}

Tags::~Tags() {
  int i;
  for (i = 0; i < stocknames.Size(); i++)
    delete[] stocknames[i];
  for (i = 0; i < NumberByLengthMulti.Size(); i++)
    delete NumberByLengthMulti[i];
  for (i = 0; i < CI.Size(); i++)
    delete CI[i];
  while (AgeLengthStock.Size() > 0)
    AgeLengthStock.Delete(0);
  while (NumBeforeEating.Size() > 0)
    NumBeforeEating.Delete(0);
  delete LgrpDiv;
}

void Tags::Reset(const TimeClass* const TimeInfo) {
  int i;
  if (TimeInfo->CurrentTime() == 1) {
    while (AgeLengthStock.Size() > 0)
      AgeLengthStock.Delete(0);
    while (NumBeforeEating.Size() > 0)
      NumBeforeEating.Delete(0);
    while (CI.Size() > 0) {
      delete CI[0];
      CI.Delete(0);
    }
    for (i = 0; i < preyindex.Size(); i++)
      preyindex[i] = -1;
    for (i = 0; i < updated.Size(); i++)
      updated[i] = 0;
  }
}

void Tags::SetStock(StockPtrVector& Stocks) {
  int i, j, found;
  StockPtrVector tempMatureStock, tempTransitionStock;
  char* stockname;

  j = 0;
  found = 0;
  while (found == 0 && j < Stocks.Size()) {
    if (strcasecmp(Stocks[j]->Name(), stocknames[0]) == 0) {
      tagstocks.resize(1, Stocks[j]);
      taggingstock = Stocks[j];
      found++;
    }
    j++;
  }
  if (found == 0) {
    cerr << "Error when searching for names of stocks for tags\n"
      << "Did not find any name matching " << stocknames[0] << endl;
    exit(EXIT_FAILURE);
  }

  //Make sure that given stocks are defined on tagarea and have same
  //lengthgroup division for the tagging experiment and the stock
  i = taggingstock->IsInArea(tagarea);
  if (i == 0) {
    cerr << "Error in tagging experiment - stock " << tagstocks[0]->Name()
      << " does not live on area: " << tagarea << endl;
    exit(EXIT_FAILURE);
  }

  const LengthGroupDivision* tempLgrpDiv = taggingstock->ReturnLengthGroupDiv();
  if (LgrpDiv->NoLengthGroups() != tempLgrpDiv->NoLengthGroups()) {
    cerr << "Error in tagging experiment - invalid length groups\n";
    exit(EXIT_FAILURE);
  }
  if (LgrpDiv->dl() != tempLgrpDiv->dl()) {
    cerr << "Error in tagging experiment - invalid length groups\n";
    exit(EXIT_FAILURE);
  }
  if (!(isZero(LgrpDiv->minLength() - tempLgrpDiv->minLength()))) {
    cerr << "Error in tagging experiment - invalid length groups\n";
    exit(EXIT_FAILURE);
  }

  preyindex.resize(1, -1);
  updated.resize(1, 0);

  if (taggingstock->DoesMove()) {
    tempTransitionStock = taggingstock->GetTransitionStocks();
    for (i = 0; i < tempTransitionStock.Size(); i++) {
      transitionstocks.resize(1, tempTransitionStock[i]);
      preyindex.resize(1, -1);
      updated.resize(1, 0);
      tagstocks.resize(1, tempTransitionStock[i]);
    }
  }

  if (taggingstock->DoesMature()) {
    tempMatureStock = taggingstock->GetMatureStocks();
    for (i = 0; i < tempMatureStock.Size(); i++) {
      maturestocks.resize(1, tempMatureStock[i]);
      found = 0;
      for (j = 0; j < transitionstocks.Size(); j++)
        if (!(strcasecmp(transitionstocks[j]->Name(), tempMatureStock[i]->Name()) == 0))
          found = 1;

      if (found == 0) {
        preyindex.resize(1, -1);
        updated.resize(1, 0);
        tagstocks.resize(1, tempMatureStock[i]);
      }
    }
  }

  for (i = 1; i < tagstocks.Size(); i++) {
    stockname = new char[strlen(tagstocks[i]->Name()) + 1];
    strcpy(stockname, tagstocks[i]->Name());
    stocknames.resize(1, stockname);
  }
}

//Must have set stocks according to stocknames using SetStock before calling Update()
//Now we need to distribute the tagged fish to the same age/length groups as the tagged stock.
void Tags::Update() {
  PopInfoVector NumberInArea;
  NumberInArea.resize(LgrpDiv->NoLengthGroups());
  PopInfo nullpop;

  int i, j;
  for (i = 0; i < NumberInArea.Size(); i++)
    NumberInArea[i] = nullpop;

  const AgeBandMatrix* stockPopInArea;
  const LengthGroupDivision* tmpLgrpDiv;
  stockPopInArea = &(taggingstock->Agelengthkeys(tagarea));
  stockPopInArea->Colsum(NumberInArea);

  //Now we have total number of stock per length in tagarea, N(., l) (NumberInArea) and
  //number of stock per age/length, N(a, l) (stockPopInArea) so we must initialize
  //AgeLengthStock so that it can hold all information of number of tagged stock
  //per area/age/length after endtime. We must make AgeBandMatrixPtrVector same size as
  //the one in stock even though have only one area entry at the beginning
  IntVector stockareas = taggingstock->Areas();
  int numareas = stockareas.Size();
  int tagareaindex = -1;
  j = 0;
  while (j <= numareas && tagareaindex == -1) {
    if (tagarea == stockareas[j])
      tagareaindex = j;
    j++;
  }
  if (tagareaindex == -1) {
    cerr << "Error - Could not calculate area while updating tagging for " << this->Name() << endl;
    exit(EXIT_FAILURE);
  }

  int maxage = stockPopInArea->Maxage();
  int minage = stockPopInArea->Minage();
  int numberofagegroups = maxage - minage + 1;
  int upperlgrp, minl, maxl, age, length, stockid;
  double numfishinarea, numstockinarea;
  IntVector sizeoflengthgroups(numberofagegroups);
  IntVector lowerlengthgroups(numberofagegroups);

  for (i = 0; i < numberofagegroups; i++) {
    lowerlengthgroups[i]= stockPopInArea->Minlength(i + minage);
    upperlgrp = stockPopInArea->Maxlength(i + minage);
    sizeoflengthgroups[i] = upperlgrp - lowerlengthgroups[i];
  }

  AgeLengthStock.resize(1, new AgeBandMatrixPtrVector(numareas, minage, lowerlengthgroups, sizeoflengthgroups));
  for (age = minage; age <= maxage; age++) {
    minl = stockPopInArea->Minlength(age);
    maxl = stockPopInArea->Maxlength(age);
    for (length = minl; length < maxl; length++) {
      numfishinarea = NumberInArea[length].N;
      numstockinarea = (*stockPopInArea)[age][length].N;
      if (numfishinarea < verysmall || numstockinarea < verysmall)
        (*AgeLengthStock[0])[tagareaindex][age][length].N = 0.0;
      else
        (*AgeLengthStock[0])[tagareaindex][age][length].N = (NumberByLength[length - minl] * numstockinarea) / numfishinarea;
    }
  }
  taggingstock->UpdateTags(AgeLengthStock[0], this, exp(-tagloss));
  updated[0] = 1;

  if (taggingstock->IsEaten()) {
    tmpLgrpDiv = taggingstock->ReturnPrey()->ReturnLengthGroupDiv();
    IntVector preysize(numberofagegroups, tmpLgrpDiv->NoLengthGroups());
    IntVector preyminlength(numberofagegroups, 0);
    NumBeforeEating.resize(1, new AgeBandMatrixPtrVector(numareas, minage, preyminlength, preysize));
    CI.resize(1);
    CI[CI.Size() - 1] = new ConversionIndex(LgrpDiv, tmpLgrpDiv);

    stockid = stockIndex(taggingstock->Name());
    if (stockid < 0 || stockid >= preyindex.Size()) {
      cerr << "Error in tagging experiment - invalid stock identifier\n";
      exit(EXIT_FAILURE);
    }
    preyindex[stockid] = NumBeforeEating.Size() - 1;
  }

  const AgeBandMatrix* allStockPopInArea;
  for (i = 1; i < tagstocks.Size(); i++) {
    Stock* tmpStock = tagstocks[i];
    allStockPopInArea = &tagstocks[i]->Agelengthkeys(tagarea);

    stockareas = tmpStock->Areas();
    numareas = stockareas.Size();
    maxage = allStockPopInArea->Maxage();
    minage = allStockPopInArea->Minage();
    numberofagegroups = maxage - minage + 1;
    while (sizeoflengthgroups.Size() > 0) {
      sizeoflengthgroups.Delete(0);
      lowerlengthgroups.Delete(0);
    }
    sizeoflengthgroups.resize(numberofagegroups);
    lowerlengthgroups.resize(numberofagegroups);
    for (j = 0; j < numberofagegroups; j++) {
      lowerlengthgroups[j] = allStockPopInArea->Minlength(j + minage);
      upperlgrp = allStockPopInArea->Maxlength(j + minage);
      sizeoflengthgroups[j] = upperlgrp - lowerlengthgroups[j];
    }

    AgeLengthStock.resize(1, new AgeBandMatrixPtrVector(numareas, minage, lowerlengthgroups, sizeoflengthgroups));
    if (tmpStock->IsEaten()) {
      tmpLgrpDiv = tmpStock->ReturnPrey()->ReturnLengthGroupDiv();
      IntVector preysize(numberofagegroups, tmpLgrpDiv->NoLengthGroups());
      IntVector preyminlength(numberofagegroups, 0);
      NumBeforeEating.resize(1, new AgeBandMatrixPtrVector(numareas, minage, preyminlength, preysize));
      CI.resize(1);
      CI[CI.Size() - 1] = new ConversionIndex(LgrpDiv, tmpLgrpDiv);

      stockid = stockIndex(tmpStock->Name());
      if (stockid < 0 || stockid >= preyindex.Size()) {
        cerr << "Error in tagging experiment - invalid stock identifier\n";
        exit(EXIT_FAILURE);
      }
      preyindex[stockid] = NumBeforeEating.Size() - 1;
    }
  }
}

void Tags::UpdateTags(int year, int step) {
  int i, timeid;

  timeid = -1;
  for (i = 0; i < Years.Size(); i++)
    if (Years[i] == year && Steps[i] == step)
      timeid = i;

  if ((timeid != -1) && (tagyear == year && tagstep == step))
    this->Update();

}

void Tags::DeleteFromStock() {
  int i;
  for (i = 0; i < tagstocks.Size(); i++) {
    if (updated[i] == 1) {
      tagstocks[i]->DeleteTags(this->Name());
      updated[i] = 2;
    }
  }
}

void Tags::UpdateMatureStock(const TimeClass* const TimeInfo) {

  int i, id;
  int currentYear = TimeInfo->CurrentYear();
  int currentStep = TimeInfo->CurrentStep();

  if (endyear <= currentYear)
    cerr << "Warning - tagging experiment is not part of the simulation anymore\n";
  else
    for (i = 0; i < maturestocks.Size(); i++) {
      id = stockIndex(maturestocks[i]->Name());
      if (id < 0 || id >= AgeLengthStock.Size()) {
        cerr << "Error in tagging experiment - invalid stock identifier" << endl;
        exit(EXIT_FAILURE);
      }
      if (updated[id] == 0) {
        maturestocks[i]->UpdateTags(AgeLengthStock[id], this, exp(-tagloss));
        updated[id] = 1;
      }
    }
}

void Tags::UpdateTransitionStock(const TimeClass* const TimeInfo) {

  int i, id;
  int currentYear = TimeInfo->CurrentYear();
  int currentStep = TimeInfo->CurrentStep();

  if (endyear <= currentYear)
    cerr << "Warning - tagging experiment is not part of the simulation anymore\n";
  else
    for (i = 0; i < transitionstocks.Size(); i++) {
      id = stockIndex(transitionstocks[i]->Name());
      if (id < 0 || id >= AgeLengthStock.Size()) {
        cerr << "Error in tagging experiment - invalid stock identifier\n";
        exit(EXIT_FAILURE);
      }
      if (updated[id] == 0) {
        transitionstocks[i]->UpdateTags(AgeLengthStock[id], this, exp(-tagloss));
        updated[id] = 1;
      }
    }
}

int Tags::stockIndex(const char* stockname) {
  int i = 0;
  while (i < stocknames.Size()) {
    if (strcasecmp(stocknames[i], stockname) == 0)
      return i;
    i++;
  }
  return -1;
}

void Tags::StoreNumberPriorToEating(int area, const char* stockname) {
  int stockid, preyid;
  stockid = stockIndex(stockname);
  if (stockid < 0) {
    cerr << "Error in tagging experiment - invalid stock identifier\n";
    exit(EXIT_FAILURE);
  }
  preyid = preyindex[stockid];
  if (preyid > NumBeforeEating.Size() || preyid < 0) {
    cerr << "Error in tagging experiment - invalid prey identifier\n";
    exit(EXIT_FAILURE);
  }
  (*NumBeforeEating[preyid])[area].SettoZero();
  AgebandmAdd((*NumBeforeEating[preyid])[area], (*AgeLengthStock[stockid])[area], *CI[preyid]);
}

const AgeBandMatrix& Tags::NumberPriorToEating(int area, const char* stockname) {
  int stockid, preyid;
  stockid = stockIndex(stockname);
  if (stockid < 0) {
    cerr << "Error in tagging experiment - invalid stock identifier\n";
    exit(EXIT_FAILURE);
  }
  preyid = preyindex[stockid];
  if (preyid > NumBeforeEating.Size() || preyid < 0) {
    cerr << "Error in tagging experiment - invalid prey identifier\n";
    exit(EXIT_FAILURE);
  }
  return (*NumBeforeEating[preyid])[area];
}

int Tags::IsWithinPeriod(int year, int step) {
  if ((year > tagyear || (year == tagyear && step >= tagstep)) && (year <= endyear))
    return 1;
  else
    return 0;
}
