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

int ghistorynbit = 12; // Number of bits used for Global History
int lhistorynbit = 10; // Number of bits used in local history


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

// Hyperceptron 
int** lPerceptron;

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
// 

void init_hyperceptron() {

    // ghr side
    int historyBits = 1 << ghistorynbit;
    gpredictors = (int*)malloc(historyBits * sizeof(int));
    for (int i = 0; i < historyBits; i++) {
        gpredictors[i] = WT;
    }
    gHistoryTable = 0;

    // local side 

    int lochistoryBits = 1 << lhistorynbit;

    lPerceptron = (int**)malloc(historyBits * sizeof(int*));
    for (int i = 0; i < historyBits; i++) {
        lPerceptron[i] = (int*)malloc(ghistorynbit * sizeof(int));
    }

    // meta side
    metaSelector = (int*)malloc(historyBits * sizeof(int));
    for (int i = 0; i < historyBits; i++) {
        metaSelector[i] = WT;
    }

    //fprintf(stderr, "Init_tournament finished");
}

int
hyper_predict_local(uint32_t pc) {


    int historyBits = 1 << ghistorynbit;
    int pc_lower_bits = pc & (historyBits - 1);
    int ghistory_lower = gHistoryTable & (historyBits - 1);

    // local choice
    int lchoice;

    //int lhistory_lower = lPatternHistoryTable[pc_lower_bits] & (historyBits - 1);
    int local = pc_lower_bits ^ (ghistory_lower);

    int sum = 0;
    
    for (int j = 0; j < ghistorynbit; j++) {
        sum += ((local % 2) * 2 - 1) * lPerceptron[pc_lower_bits][j];
        local /= 2;
    }

    if (sum >= 0)
        lchoice = TAKEN;
    else
        lchoice = NOTTAKEN;

    return lchoice;
}

void
hyper_update_local(uint32_t pc, uint8_t outcome) {

    int historyBits = 1 << ghistorynbit;
    int pc_lower_bits = pc & (historyBits - 1);
    int ghistory_lower = gHistoryTable & (historyBits - 1);

    // local choice
    int lchoice;
    //int lhistory_lower = lPatternHistoryTable[pc_lower_bits] & (historyBits - 1);
    int local = pc_lower_bits ^ (ghistory_lower);

    for (int j = 0; j < ghistorynbit; j++) {
        if (outcome) {
            lPerceptron[pc_lower_bits][j] += ((local % 2) * 2 - 1);
        }
        else {
            lPerceptron[pc_lower_bits][j] -= ((local % 2) * 2 - 1);
        }
        local /= 2;
    }

}

int hyper_predict_global(uint32_t pc) {

    int historyBits = 1 << ghistorynbit;
    int pc_lower_bits = pc & (historyBits - 1);
    int ghistory_lower = gHistoryTable & (historyBits - 1);
    int historyIndex = pc_lower_bits ^ (ghistory_lower);

    // global choice
    int gchoice;
    switch (gpredictors[historyIndex]) {
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

}

uint8_t
hyperceptron_predict(uint32_t pc) {

    int historyBits = 1 << ghistorynbit;
    int pc_lower_bits = pc & (historyBits - 1);

    // global choice
    int gchoice = hyper_predict_global(pc);
    //local choice
    int lchoice = hyper_predict_local(pc);

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
train_hyperceptron(uint32_t pc, uint8_t outcome) {
    uint32_t historyBits = 1 << ghistorynbit;
    uint32_t pc_lower_bits = pc & (historyBits - 1);
    uint32_t ghistory_lower = gHistoryTable & (historyBits - 1);

    int historyIndex = pc_lower_bits ^ (ghistory_lower);

    int gresult; // global 
    switch (gpredictors[historyIndex]) {
    case SN:
        gresult = (outcome == NOTTAKEN);
        gpredictors[historyIndex] = (outcome == TAKEN) ? WN : SN;
        break;
    case WN:
        gresult = (outcome == NOTTAKEN);
        gpredictors[historyIndex] = (outcome == TAKEN) ? WT : SN;
        break;
    case WT:
        gresult = (outcome == TAKEN);
        gpredictors[historyIndex] = (outcome == TAKEN) ? ST : WN;
        break;
    case ST:
        gresult = (outcome == TAKEN);
        gpredictors[historyIndex] = (outcome == TAKEN) ? ST : WT;
        break;
    default:
        break;
    }
    

    // local update : perceptron only updates when its prediction's wrong
    uint32_t lhistory_lower = lPatternHistoryTable[pc_lower_bits] & (historyBits - 1);
    int lresult = hyper_predict_local(pc); // Wrong or right prediction from local

    hyper_update_local(pc, outcome);

    // Update pattern histroys 
    gHistoryTable = ((1 << gHistoryTable) | outcome) & (historyBits - 1);
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
cleanup_hyperceptron() {
    free(gpredictors);
    free(metaSelector);

    int historyBits = 1 << ghistorynbit;
    for (int i = 0; i < historyBits; i++) {
        free(lPerceptron[i]);
    }
    free(lPerceptron);

    free(lBranchHistoryTable);
}


void init_tournament() {

    // ghr side
    int ghistoryBits = 1 << ghistorynbit;
    gpredictors = (int*)malloc(ghistoryBits * sizeof(int));
    for (int i = 0; i < ghistoryBits; i++) {
        gpredictors[i] = WT;
    }
    gHistoryTable = 0;

    // local side 
    int lhistoryBits = 1 << lhistorynbit;
    lPatternHistoryTable = (uint32_t*)malloc(lhistoryBits * sizeof(uint32_t));
    lBranchHistoryTable = (int*)malloc(lhistoryBits * sizeof(int));
    for (int i = 0; i < lhistoryBits; i++) {
        lPatternHistoryTable[i] = 0;
    }
    for (int i = 0; i < lhistoryBits; i++) {
        lBranchHistoryTable[i] = WT;
    }

    // meta side
    metaSelector = (int*)malloc(ghistoryBits * sizeof(int));
    for (int i = 0; i < ghistoryBits; i++) {
        metaSelector[i] = WT;
    }

    //fprintf(stderr, "Init_tournament finished");
}

uint8_t 
tournament_global_predict(uint32_t pc) {

    int ghistoryBits = 1 << ghistorynbit;
    int pc_lower_bits = pc & (ghistoryBits - 1);
    int ghistory_lower = gHistoryTable & (ghistoryBits - 1);
    int historyIndex = ghistory_lower;  //pc_lower_bits ^ (ghistory_lower);

    // global choice
    int gchoice;
    switch (gpredictors[historyIndex]) {
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
        gchoice = NOTTAKEN;
    }
    return gchoice;
}

uint8_t
tournament_local_predict(uint32_t pc) {

    int lhistoryBits = 1 << lhistorynbit;
    int pc_lower_bits = pc & (lhistoryBits - 1);
    int lhistory_lower = lPatternHistoryTable[pc_lower_bits] & (lhistoryBits - 1);

    // local choice
    int lchoice;
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
        lchoice = NOTTAKEN;
    }
    return lchoice;
}

uint8_t
tournament_predict(uint32_t pc) {
    
    int gchoice = tournament_global_predict(pc);

    int lchoice = tournament_local_predict(pc);


    int ghistoryBits = 1 << ghistorynbit;
    int pc_lower_bits = pc & (ghistoryBits - 1);
    
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

uint8_t
tournament_global_update(uint32_t pc, uint8_t outcome) {

    uint32_t ghistoryBits = 1 << ghistorynbit;
    uint32_t pc_lower_bits = pc & (ghistoryBits - 1);
    uint32_t ghistory_lower = gHistoryTable & (ghistoryBits - 1);
    uint32_t historyIndex = ghistory_lower; // pc_lower_bits ^ (ghistory_lower);

    uint8_t gchoice = tournament_global_predict(pc);

    switch (gpredictors[historyIndex]) {
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
    gHistoryTable = ((gHistoryTable << 1) | outcome) & (ghistoryBits - 1);

    return gchoice;
}

uint8_t
tournament_local_update(uint32_t pc, uint8_t outcome) {

    uint32_t lhistoryBits = 1 << lhistorynbit;
    uint32_t pc_lower_bits = pc & (lhistoryBits - 1);
    // local update : BHT + PHT
    uint32_t lhistory_lower = lPatternHistoryTable[pc_lower_bits] & (lhistoryBits - 1);

    uint8_t lchoice = tournament_local_predict(pc);

    switch (lBranchHistoryTable[lhistory_lower]) {
    case SN:
        lBranchHistoryTable[lhistory_lower] = (outcome == TAKEN) ? WN : SN;
        break;
    case WN:
        lBranchHistoryTable[lhistory_lower] = (outcome == TAKEN) ? WT : SN;
        break;
    case WT:
        lBranchHistoryTable[lhistory_lower] = (outcome == TAKEN) ? ST : WN;
        break;
    case ST:
        lBranchHistoryTable[lhistory_lower] = (outcome == TAKEN) ? ST : WT;
        break;
    default:
        break;
    }
    lPatternHistoryTable[pc_lower_bits] = ((lPatternHistoryTable[pc_lower_bits] << 1) | outcome) & (lhistoryBits - 1);
    return lchoice;
}


void
train_tournament(uint32_t pc, uint8_t outcome) {

    uint32_t ghistoryBits = 1 << ghistorynbit;
    uint32_t pc_lower_bits = pc & (ghistoryBits - 1);
    uint32_t ghistory_lower = gHistoryTable & (ghistoryBits - 1);

    uint8_t gchoice = tournament_global_update(pc, outcome);
    uint8_t lchoice = tournament_local_update(pc, outcome);

    // meta update : 
    if (gchoice != lchoice) {
        switch (metaSelector[ghistory_lower]) {
        case SN:
            metaSelector[ghistory_lower] = (lchoice == outcome) ? WN : SN;
            break;
        case WN:
            metaSelector[ghistory_lower] = (lchoice == outcome) ? WT : SN;
            break;
        case WT:
            metaSelector[ghistory_lower] = (lchoice == outcome) ? ST : WN;
            break;
        case ST:
            metaSelector[ghistory_lower] = (lchoice == outcome) ? ST : WT;
            break;
        default:
            break;
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

    ghistorynbit = 14; //little hack for reusing parameters

  int historyBits = 1 << ghistorynbit;
  gpredictors = (int*) malloc(historyBits * sizeof(int));
  for(int i = 0; i < historyBits; i++) {
    gpredictors[i] = WN;
  }
  gHistoryTable = 0;
}



uint8_t 
gshare_predict(uint32_t pc) {
  int historyBits = 1 << ghistorynbit;
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
  uint32_t historyBits = 1 << ghistorynbit;
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
  gHistoryTable = (gHistoryTable << 1) | outcome;
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
        init_hyperceptron();
        break;
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
        return hyperceptron_predict(pc);
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
        return train_hyperceptron(pc, outcome);
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
        case TOURNAMENT:
            return cleanup_tournament();
        case CUSTOM:
            return cleanup_hyperceptron();
        default:
            break;
    }

}