#include "catchintons.h"
#include "readfunc.h"
#include "readword.h"
#include "readaggregation.h"
#include "errorhandler.h"
#include "areatime.h"
#include "fleet.h"
#include "stock.h"
#include "mortpredator.h"
#include "ecosystem.h"
#include "gadget.h"

extern ErrorHandler handle;

CatchInTons::CatchInTons(CommentStream& infile, const AreaClass* const Area,
  const TimeClass* const TimeInfo, double weight, const char* name)
  : Likelihood(CATCHINTONSLIKELIHOOD, weight) {

  int i, j;
  int numarea = 0;
  int readfile = 0;
  char text[MaxStrLength];
  char datafilename[MaxStrLength];
  char aggfilename[MaxStrLength];

  strncpy(text, "", MaxStrLength);
  strncpy(datafilename, "", MaxStrLength);
  strncpy(aggfilename, "", MaxStrLength);
  ifstream datafile;
  CommentStream subdata(datafile);

  ctname = new char[strlen(name) + 1];
  strcpy(ctname, name);

  functionname = new char[MaxStrLength];
  strncpy(functionname, "", MaxStrLength);
  readWordAndValue(infile, "datafile", datafilename);
  readWordAndValue(infile, "function", functionname);

  functionnumber = 0;
  if (strcasecmp(functionname, "sumofsquares") == 0) {
    functionnumber = 1;
  } else
    handle.Message("Unknown function in catchintons");

  infile >> ws;
  char c = infile.peek();
  if ((c == 'a') || (c == 'A')) {
    infile >> text >> ws;
    if (strcasecmp(text, "aggregationlevel") == 0) {
      infile >> yearly >> ws;
    } else if (strcasecmp(text, "areaaggfile") == 0) {
      infile >> aggfilename >> ws;
      yearly = 0;
      readfile = 1;
    } else
      handle.Unexpected("aggregationlevel", text);

  } else
    yearly = 0;

  if (yearly != 0 && yearly != 1)
    handle.Message("Error in catchintons - aggregationlevel must be 0 or 1");

  //JMB - changed to make the reading of epsilon optional
  c = infile.peek();
  if ((c == 'e') || (c == 'E'))
    readWordAndVariable(infile, "epsilon", epsilon);
  else
    epsilon = 10;

  if (epsilon <= 0) {
    handle.Warning("Epsilon should be a positive number - set to default value 10");
    epsilon = 10;
  }

  if (readfile == 0)
    readWordAndValue(infile, "areaaggfile", aggfilename);

  datafile.open(aggfilename, ios::in);
  handle.checkIfFailure(datafile, aggfilename);
  handle.Open(aggfilename);
  numarea = readAggregation(subdata, areas, areaindex);
  handle.Close();
  datafile.close();
  datafile.clear();

  //Must change from outer areas to inner areas.
  for (i = 0; i < areas.Nrow(); i++)
    for (j = 0; j < areas.Ncol(i); j++)
      if ((areas[i][j] = Area->InnerArea(areas[i][j])) == -1)
        handle.UndefinedArea(areas[i][j]);

  //read in the fleetnames
  i = 0;
  infile >> text >> ws;
  if (!(strcasecmp(text, "fleetnames") == 0))
    handle.Unexpected("fleetnames", text);
  infile >> text >> ws;
  while (!infile.eof() && !(strcasecmp(text, "stocknames") == 0)) {
    fleetnames.resize(1);
    fleetnames[i] = new char[strlen(text) + 1];
    strcpy(fleetnames[i++], text);
    infile >> text >> ws;
  }

  //read in the stocknames
  i = 0;
  if (!(strcasecmp(text, "stocknames") == 0))
    handle.Unexpected("stocknames", text);
  infile >> text;
  while (!infile.eof() && !(strcasecmp(text, "[component]") == 0)) {
    infile >> ws;
    stocknames.resize(1);
    stocknames[i] = new char[strlen(text) + 1];
    strcpy(stocknames[i++], text);
    infile >> text;
  }

  //We have now read in all the data from the main likelihood file
  //But we have to read in the statistics data from datafilename
  datafile.open(datafilename, ios::in);
  handle.checkIfFailure(datafile, datafilename);
  handle.Open(datafilename);
  readCatchInTonsData(subdata, TimeInfo, numarea);
  handle.Close();
  datafile.close();
  datafile.clear();

  modelDistribution.AddRows(obsDistribution.Nrow(), numarea, 0.0);
  likelihoodValues.AddRows(obsDistribution.Nrow(), numarea, 0.0);
}

void CatchInTons::Reset(const Keeper* const keeper) {
  Likelihood::Reset(keeper);
  timeindex = 0;
  int i, j;
  for (i = 0; i < modelDistribution.Nrow(); i++)
    for (j = 0; j < modelDistribution.Ncol(i); j++)
      modelDistribution[i][j] = 0.0;
  handle.logMessage("Reset catchintons component", ctname);
}

void CatchInTons::Print(ofstream& outfile) const {
  int i;

  outfile << "\nCatch in Tons " << ctname << " - likelihood value " << likelihood
    << "\n\tFunction " << functionname;
  outfile << "\n\tStock names:";
  for (i = 0; i < stocknames.Size(); i++)
    outfile << sep << stocknames[i];
  outfile << "\n\tFleet names:";
  for (i = 0; i < fleetnames.Size(); i++)
    outfile << sep << fleetnames[i];
  outfile << endl;
  outfile.flush();
}

double CatchInTons::calcLikSumSquares(const TimeClass* const TimeInfo) {
  int a, a2, f, p;
  double totallikelihood = 0.0;
  PopPredator* pred;

  for (a = 0; a < areas.Nrow(); a++) {
    likelihoodValues[timeindex][a] = 0.0;
    for (a2 = 0; a2 < areas.Ncol(a); a2++) {
      for (f = 0; f < preyindex.Nrow(); f++) {
        pred = (PopPredator*)fleets[f]->returnPredator();
        for (p = 0; p < preyindex.Ncol(f); p++)
          modelDistribution[timeindex][a] += pred->consumedBiomass(preyindex[f][p], a2);
      }
    }

    if ((!yearly) || (TimeInfo->CurrentStep() == TimeInfo->StepsInYear())) {
      likelihoodValues[timeindex][a] +=
        (log(modelDistribution[timeindex][a] + epsilon) - log(obsDistribution[timeindex][a] + epsilon))
        * (log(modelDistribution[timeindex][a] + epsilon) - log(obsDistribution[timeindex][a] + epsilon));

      totallikelihood += likelihoodValues[timeindex][a];
    }
  }
  return totallikelihood;
}

void CatchInTons::addLikelihood(const TimeClass* const TimeInfo) {

  double l = 0.0;
  if (AAT.AtCurrentTime(TimeInfo)) {
    switch(functionnumber) {
      case 1:
        l = calcLikSumSquares(TimeInfo);
        break;
      default:
        handle.logWarning("Warning in catchintons - unknown function", functionname);
        break;
    }

    if ((!yearly) || (TimeInfo->CurrentStep() == TimeInfo->StepsInYear())) {
      likelihood += l;
      handle.logMessage("Calculating likelihood score for catchintons component", ctname);
      handle.logMessage("The likelihood score for this component on this timestep is", l);
      timeindex++;
    }
  }
}

CatchInTons::~CatchInTons() {
  int i;
  for (i = 0; i < stocknames.Size(); i++)
    delete[] stocknames[i];
  for (i = 0; i < fleetnames.Size(); i++)
    delete[] fleetnames[i];
  for (i = 0; i < areaindex.Size(); i++)
    delete[] areaindex[i];
  delete[] functionname;
  delete[] ctname;
}

void CatchInTons::setFleetsAndStocks(FleetPtrVector& Fleets, StockPtrVector& Stocks) {
  int i, j, k, found;

  for (i = 0; i < fleetnames.Size(); i++) {
    found = 0;
    for (j = 0; j < Fleets.Size(); j++)
      if (strcasecmp(fleetnames[i], Fleets[j]->Name()) == 0) {
        found++;
        fleets.resize(1, Fleets[j]);
      }

    if (found == 0)
      handle.logFailure("Error in catchintons - unknown fleet", fleetnames[i]);

  }

  for (i = 0; i < stocknames.Size(); i++) {
    found = 0;
    for (j = 0; j < Stocks.Size(); j++) {
      if (Stocks[j]->IsEaten())
        if (strcasecmp(stocknames[i], Stocks[j]->returnPrey()->Name()) == 0) {
          found++;
          stocks.resize(1, Stocks[j]);
        }
    }
    if (found == 0)
      handle.logFailure("Error in catchintons - unknown stock", stocknames[i]);

  }

  for (i = 0; i < fleets.Size(); i++) {
    found = 0;
    preyindex.AddRows(1, 0);
    Predator* pred = fleets[i]->returnPredator();
    for (j = 0; j < pred->NoPreys(); j++)
      for (k = 0; k < stocknames.Size(); k++)
        if (strcasecmp(stocknames[k], pred->Preys(j)->Name()) == 0) {
          found++;
          preyindex[i].resize(1, j);
        }

    if (found == 0)
      handle.logWarning("Warning in catchintons - found no stocks for fleet", fleetnames[i]);
  }

  if (preyindex.Nrow() != fleets.Size())
    handle.logFailure("Error in catchintons - failed to match stocks to fleets");
}

void CatchInTons::readCatchInTonsData(CommentStream& infile,
  const TimeClass* TimeInfo, int numarea) {

  int i;
  int year, step;
  double tmpnumber = 0.0;
  char tmparea[MaxStrLength];
  char tmpfleet[MaxStrLength];
  strncpy(tmparea, "", MaxStrLength);
  strncpy(tmpfleet, "", MaxStrLength);
  int keepdata, timeid, areaid, fleetid, check;
  int count = 0;

  //Find start of distribution data in datafile
  infile >> ws;
  char c = infile.peek();
  if (!isdigit(c)) {
    infile.get(c);
    while (c != '\n' && !infile.eof())
      infile.get(c);
  }

  //Check the number of columns in the inputfile
  check = countColumns(infile);
  if (!(((yearly == 1) && ((check == 4) || (check == 5))) || ((yearly == 0) && (check == 5))))
    handle.Message("Wrong number of columns in inputfile - should be 4 or 5");

  step = 1; //default value in case there are only 4 columns in the datafile
  while (!infile.eof()) {
    keepdata = 0;
    if ((yearly == 1) && (check == 4))
      infile >> year >> tmparea >> tmpfleet >> tmpnumber >> ws;
    else
      infile >> year >> step >> tmparea >> tmpfleet >> tmpnumber >> ws;

    //if tmparea is in areaindex find areaid, else dont keep the data
    areaid = -1;
    for (i = 0; i < areaindex.Size(); i++)
      if (strcasecmp(areaindex[i], tmparea) == 0)
        areaid = i;

    if (areaid == -1)
      keepdata = 1;

    //if tmpfleet is a required fleet keep the data, else ignore it
    fleetid = -1;
    for (i = 0; i < fleetnames.Size(); i++)
      if (strcasecmp(fleetnames[i], tmpfleet) == 0)
        fleetid = i;

    if (fleetid == -1)
      keepdata = 1;

    //check if the year and step are in the simulation
    timeid = -1;
    if ((TimeInfo->IsWithinPeriod(year, step)) && (keepdata == 0)) {
      //if this is a new timestep, resize to store the data
      for (i = 0; i < Years.Size(); i++)
        if ((Years[i] == year) && (yearly || (Steps[i] == step)))
          timeid = i;

      if (timeid == -1) {
        Years.resize(1, year);
	if (!(yearly))
          Steps.resize(1, step);
        timeid = (Years.Size() - 1);
        obsDistribution.AddRows(1, numarea, 0.0);
      }

    } else {
      //dont keep the data
      keepdata = 1;
    }

    if (keepdata == 0) {
      //distribution data is required, so store it
      count++;
      //note that we use += to sum the data over all fleets (and possibly time)
      obsDistribution[timeid][areaid] += tmpnumber;
    }
  }

  if (yearly)
    AAT.addActionsAllSteps(Years, TimeInfo);
  else
    AAT.addActions(Years, Steps, TimeInfo);

  if (count == 0)
    handle.logWarning("Warning in catchintons - found no data in the data file for", ctname);
  handle.logMessage("Read catchintons data file - number of entries", count);
}

void CatchInTons::LikelihoodPrint(ofstream& outfile) {
  int y, a, i;

  outfile << "\nCatch in Tons\n\nLikelihood " << likelihood << "\nFunction "
    << functionname << "\nWeight " << weight << "\nStock names:";
  for (i = 0; i < stocknames.Size(); i++)
    outfile << sep << stocknames[i];
  outfile << "\nFleet names:";
  for (i = 0; i < fleetnames.Size(); i++)
    outfile << sep << fleetnames[i];
  outfile << "\n\nLandings data:\n";

  for (y = 0; y < Years.Size(); y++) {
    outfile << "\nYear " << Years[y];
    if (!(yearly))
      outfile << " and step " << Steps[y];

    for (a = 0; a < obsDistribution.Ncol(y); a++)
      outfile << "\nInternal area: " << a << "\nMeasured catch is " << obsDistribution[y][a]
        << " modelled catch is " << modelDistribution[y][a] << " likelihood value " << likelihoodValues[y][a];

    outfile << endl;
  }
  outfile.flush();
}
