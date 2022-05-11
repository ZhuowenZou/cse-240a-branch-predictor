//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <stdlib.h>
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

int ghistorynbit = 12; // c: Number of bits used for Global History
int lpredictionnbit = 10; // b: Number of bits used for local prediction
int lhistorynbit = 11; // a: Number of bits used for local history

int bpType;       // Branch Prediction Type
int verbose;

int perceptron_len = 1024 * 32 / 9;
int perceptron_precision = 8;


int perc_tableSize = 9;
int magic_prime = 509; // 509, 1021, 2039,  

int perc_ghistnbit = 16;
int perc_precision = 4;

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
int8_t* perceptron;
uint8_t* signature;

int perc_max, perc_min;

// Perceptron 
int8_t** perceptronTable;
uint32_t ghr;


//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
// 

void init_perceptron() {

    // ghr side

    perceptronTable = (int8_t**)malloc( (1 << perc_tableSize) * sizeof(int8_t*));
    for (int i = 0; i < (1 << perc_tableSize); i++) {
        perceptronTable[i] = (int8_t*)malloc((perc_ghistnbit) * sizeof(int8_t));
        for (int j = 0; j < perc_ghistnbit; j++) {
            perceptronTable[i][j] = 0;
        }
    }

    ghr = 0;
    perc_max = 1 << (perc_precision - 1);
    perc_min = -1 * (1 << (perc_precision - 1));
}


uint8_t
perceptron_predict(uint32_t pc) {

    int ghistoryBits = 1 << perc_ghistnbit;
    int ghistory_lower = ghr & (ghistoryBits - 1);

    int addr = pc % magic_prime;
    int8_t* perceptronEntry = perceptronTable[addr];

    int sum = 0;
    uint32_t temp = ghistory_lower;
    // Computes perceptron
    for (int i = 0; i < perc_ghistnbit; i++) {
        sum += perceptronEntry[i] * (2 * (temp % 2) - 1);
        temp = temp / 2;
    }

    uint8_t choice;
    if (sum >= 0)
        choice = TAKEN;
    else
        choice = NOTTAKEN;

    return choice;
}

void
train_perceptron(uint32_t pc, uint8_t outcome) {


    uint8_t choice = perceptron_predict(pc);

    int ghistoryBits = 1 << perc_ghistnbit;
    int ghistory_lower = ghr & (ghistoryBits - 1);

    int addr = pc % magic_prime;
    int8_t* perceptronEntry = perceptronTable[addr];

    if (choice != outcome) {

        uint32_t temp = ghistory_lower;

        // Computes perceptron
        for (int i = 0; i < perc_ghistnbit; i++) {

            int direction = ( (temp % 2) * 2 - 1) * ((outcome == TAKEN) ? 1 : -1);

            if ((direction > 0) && (perceptronEntry[i] < perc_max) || (direction < 0) && (perceptronEntry[i] > perc_min))
                perceptronEntry[i] += direction;

            temp = temp / 2;
        }
    }

    ghr = ((ghr << 1) | outcome) & (ghistoryBits - 1);

}


void
cleanup_perceptron() {
    for (int i = 1; i < (1 << perc_tableSize); i++) {
        free(perceptronTable[i]);
    }
    free(perceptronTable);
}

void init_hyperceptron() {

    perceptron = (int8_t*)malloc(perceptron_len * sizeof(int8_t));
    signature = (uint8_t*)malloc(perceptron_len * sizeof(uint8_t));

    for (int i = 0; i < perceptron_len; i++) {
        perceptron[i] = 0;
        signature[i] = 0;
    }

    ghr = 0;

    perc_max = 1 << (perceptron_precision - 1);
    perc_min = -1 * (1 << (perceptron_precision - 1) - 1);
}

uint32_t
pc_hash(uint32_t pc) {
    srand(pc);
    return rand();
}

uint32_t
ghr_hash(uint32_t ghr) {
    srand(ghr);
    return rand();
}

uint8_t
hyperceptron_predict(uint32_t pc) {

    int historyBits = 1 << ghistorynbit;
    int ghistory_lower = ghr & (historyBits - 1);

    for (int i = 0; i < perceptron_len; i++) {
        signature[i] = 0;
    }

    // Commputes signature 
    int idx = 0;
    while (idx < perceptron_len) {
        uint32_t newpc_hash = pc_hash(pc + idx);
        uint32_t newghr_hash = ghr_hash(ghistory_lower + idx);
        uint32_t bind = newpc_hash ^ newghr_hash; 

        for (int i = 0; i < 32; i++) {
            signature[idx] = bind % 2;
            bind = bind / 2;
            idx++;
            if (idx = perceptron_len) {
                break;
            }
        }
    }

    int sum = 0;
    // Computes perceptron
    for (int i = 0; i < perceptron_len; i++) 
        sum += perceptron[i] * (2 * signature[i] - 1);
    
    uint8_t choice;
    if (sum >= 0)
        choice = TAKEN;
    else
        choice = NOTTAKEN;

    return choice;
}

void
train_hyperceptron(uint32_t pc, uint8_t outcome) {
   

    uint8_t choice = hyperceptron_predict(pc);
    
    if (choice != outcome) {

        // Computes perceptron
        for (int i = 0; i < perceptron_len; i++) {

            int direction = (signature[i] * 2 - 1) * ((outcome == TAKEN) ? 1 : -1);

            if ((direction > 0) && (perceptron[i] < perc_max) || (direction < 0) && (perceptron[i] > perc_min))
                perceptron[i] += direction;


        }
    }
    int historyBits = 1 << ghistorynbit;
    ghr = ((ghr << 1) | outcome) & (historyBits - 1);

}

void
cleanup_hyperceptron() {
    free(perceptron);
    free(signature);
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
    for (int i = 0; i < lhistoryBits; i++) {
        lPatternHistoryTable[i] = 0;
    }

    int lpredictionBits = 1 << lpredictionnbit;
    lBranchHistoryTable = (int*)malloc(lpredictionBits * sizeof(int));
    for (int i = 0; i < lpredictionBits; i++) {
        lBranchHistoryTable[i] = WN;
    }

    // meta side
    metaSelector = (int*)malloc(ghistoryBits * sizeof(int));
    for (int i = 0; i < ghistoryBits; i++) {
        metaSelector[i] = WT;
    }

    //fprintf(stderr, "Init_tournament finished");
}


void init_tournament2() {


    ghistorynbit = 10; // c: Number of bits used for Global History
    lpredictionnbit = 11; // b: Number of bits used for local prediction
    lhistorynbit = 11; // a: Number of bits used for local history

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
    for (int i = 0; i < lhistoryBits; i++) {
        lPatternHistoryTable[i] = 0;
    }

    int lpredictionBits = 1 << lpredictionnbit;
    lBranchHistoryTable = (int*)malloc(lpredictionBits * sizeof(int));
    for (int i = 0; i < lpredictionBits; i++) {
        lBranchHistoryTable[i] = WN;
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

void
tournament_global_update(uint32_t pc, uint8_t outcome) {

    uint32_t ghistoryBits = 1 << ghistorynbit;
    uint32_t ghistory_lower = gHistoryTable & (ghistoryBits - 1);
    uint32_t historyIndex = ghistory_lower; // pc_lower_bits ^ (ghistory_lower);

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
}

void
tournament_local_update(uint32_t pc, uint8_t outcome) {

    uint32_t lhistoryBits = 1 << lhistorynbit;
    uint32_t lpredictionBits = 1 << lpredictionnbit;

    uint32_t pc_lower_bits = pc & (lhistoryBits - 1);
    uint32_t lhistory_lower = lPatternHistoryTable[pc_lower_bits] & (lpredictionBits - 1);
    
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
}

void
train_tournament(uint32_t pc, uint8_t outcome) {

    uint32_t ghistoryBits = 1 << ghistorynbit;
    uint32_t pc_lower_bits = pc & (ghistoryBits - 1);
    uint32_t ghistory_lower = gHistoryTable & (ghistoryBits - 1);

    uint32_t lhistoryBits = 1 << lhistorynbit;
    uint32_t lpc_lower_bits = pc & (lhistoryBits - 1);
    uint32_t lpredictionBits = 1 << lpredictionnbit;

    uint8_t gchoice = tournament_global_predict(pc);
    uint8_t lchoice = tournament_local_predict(pc);

    tournament_global_update(pc, outcome);
    tournament_local_update(pc, outcome);

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

    // update state tables
    lPatternHistoryTable[lpc_lower_bits] = ((lPatternHistoryTable[lpc_lower_bits] << 1) | outcome) & (lpredictionBits - 1);
    gHistoryTable = ((gHistoryTable << 1) | outcome) & (ghistoryBits - 1);

}

void
cleanup_tournament() {
    free(gpredictors);
    free(metaSelector); 
    free(lPatternHistoryTable);
    free(lBranchHistoryTable);
}


//tournament2

uint8_t
tournament2_global_predict(uint32_t pc) {

    int ghistoryBits = 1 << ghistorynbit;
    int pc_lower_bits = pc & (ghistoryBits - 1);
    int ghistory_lower = gHistoryTable & (ghistoryBits - 1);
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
        gchoice = NOTTAKEN;
    }
    return gchoice;
}

uint8_t
tournament2_predict(uint32_t pc) {

    int gchoice = tournament2_global_predict(pc);

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

void
tournament2_global_update(uint32_t pc, uint8_t outcome) {

    uint32_t ghistoryBits = 1 << ghistorynbit;
    uint32_t ghistory_lower = gHistoryTable & (ghistoryBits - 1);
    uint32_t pc_lower_bits = pc & (ghistoryBits - 1);
    uint32_t historyIndex = pc_lower_bits ^ (ghistory_lower);

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
}

void
train_tournament2(uint32_t pc, uint8_t outcome) {

    uint32_t ghistoryBits = 1 << ghistorynbit;
    uint32_t pc_lower_bits = pc & (ghistoryBits - 1);
    uint32_t ghistory_lower = gHistoryTable & (ghistoryBits - 1);

    uint32_t lhistoryBits = 1 << lhistorynbit;
    uint32_t lpc_lower_bits = pc & (lhistoryBits - 1);
    uint32_t lpredictionBits = 1 << lpredictionnbit;

    uint8_t gchoice = tournament2_global_predict(pc);
    uint8_t lchoice = tournament_local_predict(pc);

    tournament2_global_update(pc, outcome);
    tournament_local_update(pc, outcome);

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

    // update state tables
    lPatternHistoryTable[lpc_lower_bits] = ((lPatternHistoryTable[lpc_lower_bits] << 1) | outcome) & (lpredictionBits - 1);
    gHistoryTable = ((gHistoryTable << 1) | outcome) & (ghistoryBits - 1);

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
        init_perceptron();
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
        return perceptron_predict(pc);
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
        return train_perceptron(pc, outcome);
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
            return cleanup_perceptron();
        default:
            break;
    }

}