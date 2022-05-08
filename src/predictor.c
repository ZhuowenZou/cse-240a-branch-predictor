//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <math.h>
#include "predictor.h"

//
// My Student Information
//
const char *studentName = "Zhuowen Zou";
const char *studentID   = "A14554030";
const char *email       = "zhz402@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

//define number of bits required for indexing the BHT here. 
int ghistoryBits = 10; // Number of bits used for Global History

int lhistoryBits = 4; // Number of bits used for Local History Table 

int bpType;       // Branch Prediction Type
int verbose;




//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
//TODO: Add your own Branch Predictor data structures here
//
//gshare
int *gpredictors; // global BHT
int gHistoryTable; // ghr


//Tournament
// int* gpredictors; 
// int gHistoryTable;
int* lPatternHistoryTable; 
int* lBranchHistoryTable; 

int* metaSelector; //select the meta



//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
// 

void init_tournament() {

    // ghr side
    int historyBits = 1 << ghistoryBits;
    gpredictors = (int*)malloc(historyBits * sizeof(int));
    for (int i = 0; i < historyBits; i++) {
        gpredictors[i] = WT;
    }
    gHistoryTable = 0;

    // local side 

    lPatternHistoryTable = (uint32_t*)malloc(historyBits * sizeof(uint32_t));
    lBranchHistoryTable = (int*)malloc(historyBits * sizeof(int));
    for (int i = 0; i < historyBits; i++) {
        lPatternHistoryTable[i] = 0;
    }
    for (int i = 0; i < historyBits; i++) {
        lBranchHistoryTable[i] = WT;
    }

    // meta side
    metaSelector = (int*)malloc(historyBits * sizeof(int));
    for (int i = 0; i < historyBits; i++) {
        metaSelector[i] = WT;
    }

    //fprintf(stderr, "Init_tournament finished");
}

uint8_t
tournament_predict(uint32_t pc) {
    int historyBits = 1 << ghistoryBits;
    int pc_lower_bits = pc & (historyBits - 1);
    int ghistory_lower = gHistoryTable & (historyBits - 1);

    // global choice
    int gchoice;
    switch (gpredictors[pc_lower_bits]) {
    case SN:
    case WN:
        gchoice = NOTTAKEN;
        break;
    case WT:
    case ST:
        gchoice = TAKEN;
        break;
    default:
        printf("Undefined state in global predictor table");
        return NOTTAKEN;
    }

    // local choice
    int lchoice; 
    int lhistory_lower = lPatternHistoryTable[pc_lower_bits] & (historyBits - 1);
    switch (lBranchHistoryTable[lhistory_lower]) {
    case SN:
    case WN:
        lchoice = NOTTAKEN;
        break;
    case WT:
    case ST:
        lchoice = TAKEN;
        break;
    default:
        printf("Undefined state in local predictor table");
        return NOTTAKEN;
    }

    //meta choice
    switch (metaSelector[pc_lower_bits]) {
        case SN:
        case WN:
            return gchoice;
        case WT:
        case ST:
            return lchoice;
        default:
            printf("Undefined state in meta selector");
            return gchoice;
    }

}

void
train_tournament(uint32_t pc, uint8_t outcome) {
    uint32_t historyBits = 1 << ghistoryBits;
    uint32_t pc_lower_bits = pc & (historyBits - 1);
    uint32_t ghistory_lower = gHistoryTable & (historyBits - 1);

    int gresult; // global 
    switch (gpredictors[pc_lower_bits]) {
    case SN:
        gresult = (outcome == NOTTAKEN);
        gpredictors[pc_lower_bits] = (outcome == TAKEN) ? WN : SN;
        break;
    case WN:
        gresult = (outcome == NOTTAKEN);
        gpredictors[pc_lower_bits] = (outcome == TAKEN) ? WT : SN;
        break;
    case WT:
        gresult = (outcome == TAKEN);
        gpredictors[pc_lower_bits] = (outcome == TAKEN) ? ST : WN;
        break;
    case ST:
        gresult = (outcome == TAKEN);
        gpredictors[pc_lower_bits] = (outcome == TAKEN) ? ST : WT;
        break;
    default:
        break;
    }
    gHistoryTable = ((1 << gHistoryTable) | outcome) & (historyBits - 1);

    // local update : BHT + PHT
    uint32_t lhistory_lower = lPatternHistoryTable[pc_lower_bits] & (historyBits - 1);
    int lresult; // Wrong or right prediction from local

    switch (lBranchHistoryTable[lhistory_lower]) {
    case SN:
        lresult = (outcome == NOTTAKEN);
        lBranchHistoryTable[lhistory_lower] = (outcome == TAKEN) ? WN : SN;
        break;
    case WN:
        lresult = (outcome == NOTTAKEN);
        lBranchHistoryTable[lhistory_lower] = (outcome == TAKEN) ? WT : SN;
        break;
    case WT:
        lresult = (outcome == TAKEN);
        lBranchHistoryTable[lhistory_lower] = (outcome == TAKEN) ? ST : WN;
        break;
    case ST:
        lresult = (outcome == TAKEN);
        lBranchHistoryTable[lhistory_lower] = (outcome == TAKEN) ? ST : WT;
        break;
    default:
        break;
    }
    lPatternHistoryTable[pc_lower_bits] = ((1 << lPatternHistoryTable[pc_lower_bits]) | outcome) & (historyBits - 1);

    // meta update : 
    if (gresult != lresult) {
        if (lresult) {
            switch (metaSelector[pc_lower_bits]) {
            case SN:
                metaSelector[pc_lower_bits] = (outcome == TAKEN) ? WN : SN;
                break;
            case WN:
                metaSelector[pc_lower_bits] = (outcome == TAKEN) ? WT : SN;
                break;
            case WT:
                metaSelector[pc_lower_bits] = (outcome == TAKEN) ? ST : WN;
                break;
            case ST:
                metaSelector[pc_lower_bits] = (outcome == TAKEN) ? ST : WT;
                break;
            default:
                break;
            }
        }
        else {
            switch (metaSelector[pc_lower_bits]) {
            case SN:
                metaSelector[pc_lower_bits] = (outcome == NOTTAKEN) ? WN : SN;
                break;
            case WN:
                metaSelector[pc_lower_bits] = (outcome == NOTTAKEN) ? WT : SN;
                break;
            case WT:
                metaSelector[pc_lower_bits] = (outcome == NOTTAKEN) ? ST : WN;
                break;
            case ST:
                metaSelector[pc_lower_bits] = (outcome == NOTTAKEN) ? ST : WT;
                break;
            default:
                break;
            }
        }
    }

}

void
cleanup_tournament() {
    free(gpredictors);
    free(metaSelector); 
    free(lPatternHistoryTable);
    free(lBranchHistoryTable);
}


//gshare functions
void init_gshare() {
  int historyBits = 1 << ghistoryBits;
  gpredictors = (int*) malloc(historyBits * sizeof(int));
  for(int i = 0; i <= historyBits; i++) {
    gpredictors[i] = WN;
  }
  gHistoryTable = 0;
}



uint8_t 
gshare_predict(uint32_t pc) {
  int historyBits = 1 << ghistoryBits;
  int pc_lower_bits = pc & (historyBits - 1);
  int ghistory_lower = gHistoryTable & (historyBits - 1);
  int historyIndex = pc_lower_bits ^ (ghistory_lower);

  switch(gpredictors[historyIndex]) {
    case SN:
      return NOTTAKEN;
    case WN:
      return NOTTAKEN;
    case WT:
      return TAKEN;
    case ST:
      return TAKEN;
    default:
      printf("Undefined state in predictor table");
      return NOTTAKEN;
  }
}

void
train_gshare(uint32_t pc, uint8_t outcome) {
  uint32_t historyBits = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (historyBits - 1);
  uint32_t ghistory_lower = gHistoryTable & (historyBits - 1);
  uint32_t historyIndex = pc_lower_bits ^ (ghistory_lower);

  switch(gpredictors[historyIndex]) {
    case SN:
      gpredictors[historyIndex] = (outcome == TAKEN) ? WN : SN;
      break;
    case WN:
      gpredictors[historyIndex] = (outcome == TAKEN) ? WT : SN;
      break;
    case WT:
      gpredictors[historyIndex] = (outcome == TAKEN) ? ST : WN;
      break;
    case ST:
      gpredictors[historyIndex] = (outcome == TAKEN) ? ST : WT;
      break;
    default:
      break;
  }
  gHistoryTable = (1 << gHistoryTable) | outcome;
}

void
cleanup_gshare() {
  free(gpredictors);
}



void
init_predictor()
{
  switch (bpType) {
    case STATIC:
    case GSHARE:
        init_gshare();
        break;
    case TOURNAMENT:
        init_tournament();
        break;
    case CUSTOM:
    default:
      break;
  }
  
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t
make_prediction(uint32_t pc)
{

  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
        return TAKEN;
    case GSHARE:
        return gshare_predict(pc);
    case TOURNAMENT:
        return tournament_predict(pc);
    case CUSTOM:
    default:
      break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//

void
train_predictor(uint32_t pc, uint8_t outcome)
{

  switch (bpType) {
    case STATIC:
    case GSHARE:
        return train_gshare(pc, outcome);
    case TOURNAMENT:
        return train_tournament(pc, outcome);
    case CUSTOM:
    default:
      break;
  }
 
}

void
free_predictor()
{
    switch (bpType) {
        case STATIC:
            break;
        case GSHARE:
            return cleanup_gshare();
            break;
        case TOURNAMENT:
            return cleanup_tournament();
            break;
        case CUSTOM:
        default:
            break;
    }

}