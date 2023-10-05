#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include "rp.h"

// Function prototypes
struct sigInfo;
struct validInfo;
bool initializeRP();
bool acquireSignal(float **buff_ch1, float **buff_ch2, uint32_t *buff_size);
struct sigInfo processSignal(float *buff_ch1, 
                    float *buff_ch2, 
                    uint32_t buff_size,
                    float B_baseline,
                    float A_baseline,
                    float unittime,
                    float center_finetuning,
                    float gain,
                    float decay);
struct validInfo validateResults(double center, 
                    double delta_PC, 
                    float intensity, 
                    float *buff_ch1, 
                    float *buff_ch2,
                    float gain,
                    float decay, 
                    uint32_t buff_size)


int main(int argc, char **argv) {
    // USER PARAMS
    float B_baseline = 0.25;
    float A_baseline = -0.036;  
    float unittime = 4192.0/16384;
    float unitfreq = unittime * 4/2000;
    int trig_time = 100/unittime;
    float center_finetuning = 4340;
    float gain = -0.0001;
    float decay = 0.7;
    
    struct sigInfo result;
    struct validInfo validResults;

    // check RP initialization
    if (!initializeRP()) {
        return 1;
    }
    // perform memory allocation
    uint32_t buff_size = 16384;
    uint32_t buff_size_trim = 8192;
    float *buff_ch1 = (float *)malloc(buff_size * sizeof(float));
    float *buff_ch2 = (float *)malloc(buff_size * sizeof(float));
    // open up write files
    FILE *fpa = fopen("ch1.txt", "w+");
    FILE *fpb = fopen("ch2.txt", "w+");

    // set RP trigger levels and decimation
    rp_AcqReset();
    rp_AcqSetDecimation(RP_DEC_32);
    rp_AcqSetTriggerLevel(RP_CH_1, 0.7);
    // acquire signals
    while (1) {
        if (!acquireSignal(&buff_ch1, &buff_ch2, &buff_size)) {
            break;
        }
        // signals should now be acquired and have populated buffers
        clock_t begin clock();
        result = processSignal(&buff_ch1, 
                        &buff_ch2, 
                        buff_size, 
                        B_baseline, 
                        A_baseline, 
                        unittime, 
                        center_finetuning,
                        gain, 
                        decay);
        // validate results
        validResults = validateResults(result.center, 
                                        result.delta_PC, 
                                        result.intensity, 
                                        &buff_ch1, 
                                        &buff_ch2,
                                        gain,
                                        decay, 
                                        buff_size);

        clock_t end = clock();
        // look at time spent
        double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
        printf("\n time spent = %f ", time_spent);

        if (validResults.valid_run){
            printf("valid run! \n");
            printf("Delta_PC = %f MHz. ", result.delta_PC);
            printf("intensity = %f, ", result.intensity);
            printf("history = %f. \n", validResults.history);
            printf("output = %f. \n", validResults.output);    
        }

        else {
            printf("INVALID run!!! \n");
            if (validResults.front==0) printf("front of sequnce has no signal.\n");
            if (validResults.rear==0) printf("rear of sequnce has no signal.\n");
            if (validResults.front < 20 && rear < 20) printf("cavity most likely unlocked.\n");
            if (results.intensity > 1000) printf("SPCM saturated.\n");
        }       
    }
    
    // Clean up and release resources
    fclose(fpa);
    fclose(fpb);
    free(buff_ch1);
    free(buff_ch2);
    rp_Release();

    return 0;
}

// Function to check if Red Pitaya API initialization was successful
bool initializeRP() {
    if (rp_Init() != RP_OK) {
        fprintf(stderr, "Rp API init failed!\n");
        return false;
    }
    return true;
}

// Function to acquire signal data
bool acquireSignal(float **buff_ch1, float **buff_ch2, uint32_t *buff_size) {
    rp_AcqStart();
    // sleep for a bit post acquisition to get fresh buffers
    sleep(0.6);
    rp_AcqSetTriggerSrc(RP_TRIG_SRC_CHA_PE);
    // ensure RP is properly triggered
    rp_acq_trig_state_t state = RP_TRIG_STATE_WAITING;
    while (1) {
        rp_AcqGetTriggerState(&state);
        if (state == RP_TRIG_STATE_TRIGGERED) {
            break;
        }
    }

    uint32_t pos = 0;
    rp_AcqGetWritePointerAtTrig(&pos);
    // populate buffers with data
    rp_AcqGetDataV2(pos, buff_size, *buff_ch1, *buff_ch2);

    return true;
}

// make a struct for the signal
struct sigInfo{
    double center;
    double delta_PC;
    float intensity;
}

// Function to process the acquired signal
struct sigInfo processSignal(float *buff_ch1, 
                    float *buff_ch2, 
                    uint32_t buff_size,
                    float B_baseline,
                    float A_baseline,
                    float unittime,
                    float center_finetuning,
                    float gain,
                    float decay,
                    float unitfreq) {
    // initialize relevant variables
    int counter = 0;
    float numerator = 0.0;
    float denominator = 0.0;
    //float A_sum = 0.0;
    float B_sum = 0.0;
    float sum_temp = 0.0;
    bool valid_run = true;
    struct sigInfo result;

    // compute B_baseline to be average value of buffer
    for(int i=buff_size_trim+1000;i<buff_size;i++){ 
            sum_temp+=buff_ch2[i]; 
        }
    B_baseline =sum_temp/(buff_size-buff_size_trim-1000);

    for(int i = trig_time; i < buff_size_trim; i++){
        // count number of signals greater than 0.1
        if (buff_ch1[i] >0.1) counter++;
        // set numerator and denominator and process signal from buff_ch2
        numerator += i * (buff_ch2[i]-B_baseline) * (buff_ch2[i]-B_baseline);
        denominator += (buff_ch2[i]-B_baseline) * (buff_ch2[i]-B_baseline);
        // compute deviation of each signal from baseline
        B_sum +=  buff_ch2[i]-B_baseline;
        }

    // set center and delta_PC frequencies and return struct containing these
    result.center = unittime * (numerator/denominator - center_finetuning);
    result.delta_PC = unitfreq * (numerator/denominator - center_finetuning);
    result.intensity = denominator;
    return result;
}

struct validInfo{
    bool valid_run;
    int front;
    int rear;
    double history;
    double output;
}
// function to validate results
struct validInfo validateResults(double center, 
                    double delta_PC, 
                    float intensity, 
                    float *buff_ch1, 
                    float *buff_ch2,
                    float gain,
                    float decay, 
                    uint32_t buff_size){
    // initialize relevant variables
    bool valid_run = true;
    float thresh = 0.2;
    int front = 0;
    int rear = 0;
    double history = 0.0;
    double output = 0.0;

    // check if front and rear of signals > 0.2
    for (int i = trig_time; i < trig_time+1000; i++){
        if (buff_ch2[i]>thresh) front += 1;
    }

    for(int i = buff_size_trim-1000; i < buff_size_trim; i++){
            if (buff_ch2[i]>thresh) rear += 1;
    }
    // if number of signals less than 20 in front or rear of buffer
    // or intensity > 1000, than run is a fail
    if (front < 20 && rear < 20) valid_run = false;
    if (front == 0 || rear == 0) valid_run = false;
    if (intensity > 1000) valid_run = false;
    if (valid_run){
        history = decay * history + center;
        output += history * gain;
        }
    // check if output is hitting rails
    if (output > 1){
        printf("Hitting upper rail! ");
        output = 1;
        }
    if (output < -1){
        printf("Hitting lower rail! ");
        output = -1;
        }

    rp_GenReset();
    
    if (output > 0) rp_GenWaveform(RP_CH_1, RP_WAVEFORM_DC);
    else rp_GenWaveform(RP_CH_1, RP_WAVEFORM_DC_NEG);
    rp_GenAmp(RP_CH_1, fabs(output));
    rp_GenOutEnable(RP_CH_1);
    
    // populate struct with results
    struct validInfo validResults;
    validResults.valid_run = valid_run;
    validResults.front = front;
    validResults.rear = rear;
    validResults.history = history;
    validResults.output = output;

    return validResults;
    }


