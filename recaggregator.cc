#include "recaggregator.h"
#include "areatime.h"
#include "errorhandler.h"
#include "readfunc.h"
#include "stock.h"
#include "stockprey.h"
#include "mortprey.h"
#include "lengthpredator.h"
#include "fleet.h"
#include "readword.h"
#include "gadget.h"

RecAggregator::RecAggregator(const FleetPtrVector& Fleets,
  const StockPtrVector& Stocks, LengthGroupDivision* const Lgrpdiv,
  const IntMatrix& Areas, const IntMatrix& Ages, Tags* tag)
  : fleets(Fleets), stocks(Stocks), LgrpDiv(Lgrpdiv),
    areas(Areas), ages(Ages) {

  taggingExp = tag;
  int i;
  for (i = 0; i < stocks.Size(); i++) {
    checkLengthGroupIsFiner(stocks[i]->ReturnPrey()->ReturnLengthGroupDiv(),
      LgrpDiv, stocks[i]->Name(), "recapture consumption");
    CI.resize(1);
    CI[i] = new ConversionIndex(stocks[i]->ReturnPrey()->ReturnLengthGroupDiv(), LgrpDiv);
  }

  //Resize totalcatch using dummy variables tmppop and popmatrix.
  PopInfo tmppop;
  tmppop.N = 1.0;
  tmppop.W = 1.0;
  PopInfoMatrix popmatrix(ages.Nrow(), LgrpDiv->NoLengthGroups(), tmppop);
  totalcatch.resize(areas.Nrow(), 0, 0, popmatrix);
}

RecAggregator::~RecAggregator() {
  int i;
  for (i = 0; i < CI.Size(); i++)
    delete CI[i];
}

void RecAggregator::Print(ofstream& outfile) const {
  int i, j, k;

  for (i = 0; i < totalcatch.Size(); i++) {
    outfile << "\tInternal areas " << 1 << endl;
    for (j = 0; j < totalcatch[i].Nrow(); j++) {
      outfile << TAB;
      for (k = 0; k < totalcatch[i].Maxlength(j); k++) {
        outfile.width(smallwidth);
        outfile << totalcatch[i][j][k].N;
        outfile << sep;
      }
      outfile << endl;
    }
  }
  outfile << flush;
}

void RecAggregator::Sum(const TimeClass* const TimeInfo) {

  int i, j, k, f, h, z;
  int aggrArea, aggrAge, area, age;
  double fleetscale;
  PopInfo nullpop;

  //Initialize totalcatch to 0
  for (i = 0; i < totalcatch.Size(); i++)
    for (j = 0; j < totalcatch[i].Nrow(); j++)
      for (k = 0; k < totalcatch[i].Maxlength(j); k++)
        totalcatch[i][j][k] = nullpop;

  //Sum over the appropriate fleets, stocks, areas, ages and length groups.
  //The index aggrArea is for the dummy area in totalcatch.
  //The index aggrAge is for the dummy age in totalcatch.
  for (f = 0; f < fleets.Size(); f++) {
    LengthPredator* pred = fleets[f]->ReturnPredator();
    for (h = 0; h < stocks.Size(); h++) {
      //AJ 06.06.00 Typecast Prey which is returned from ReturnPrey to (StockPrey*)
      StockPrey* prey = (StockPrey*)stocks[h]->ReturnPrey();
      for (aggrArea = 0; aggrArea < areas.Nrow(); aggrArea++) {
        for (j = 0; j < areas.Ncol(aggrArea); j++) {
          //All the areas in areas[aggrArea] will be aggregated to the
          //area aggrArea in totalcatch.
          area = areas[aggrArea][j];
          if (prey->IsInArea(area) && fleets[f]->IsInArea(area)) {
            fleetscale = fleets[f]->Amount(area, TimeInfo) * pred->Scaler(area);
            if (fleets[f]->Type() == LINEARFLEET)
              fleetscale *= TimeInfo->LengthOfCurrent() / TimeInfo->LengthOfYear();

            for (i = 0; i < pred->NoPreys(); i++) {
              if (prey->Name() == pred->Preys(i)->Name()) {
                const DoubleIndexVector* suitptr = &pred->Suitability(i)[0];
                const AgeBandMatrix* alptr = &taggingExp->NumberPriorToEating(area, stocks[h]->Name());
                for (aggrAge = 0; aggrAge < ages.Nrow(); aggrAge++) {
                  for (k = 0; k < ages.Ncol(aggrAge); k++) {
                    age = ages[aggrAge][k];
                    if ((alptr->Minage() <= age) && (age <= alptr->Maxage())) {

                      DoubleIndexVector Ratio = *suitptr;
                      for (z = Ratio.Mincol(); z < Ratio.Maxcol(); z++)
                        Ratio[z] *= (prey->Ratio(area, z) > 1 ? 1 / prey->Ratio(area, z) : 1);

                      PopinfoAdd(totalcatch[aggrArea][aggrAge], (*alptr)[age], *CI[h], fleetscale, Ratio);
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
