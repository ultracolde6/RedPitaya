/* Red Pitaya C API example Acquiring a signal from a buffer
 * This application acquires a signal on a specific channel */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include "rp.h"


int main(int argc, char **argv){
        // initialize the red pitaya
        if(rp_Init() != RP_OK){
                fprintf(stderr, "Rp api init failed!\n");
        }
        /*LOOB BACK FROM OUTPUT 2 - ONLY FOR TESTING*/
        //rp_GenReset();
        //rp_GenFreq(RP_CH_1, 20000.0);
        //rp_GenAmp(RP_CH_1, 1.0);
        //rp_GenWaveform(RP_CH_1, RP_WAVEFORM_SINE);
        //rp_GenOutEnable(RP_CH_1);

        // allocate
        uint32_t buff_size = 16384;
        uint32_t buff_size_trim = 8192;

        // set red pitaya trigger levels and decimation
        rp_AcqReset();
        rp_AcqSetDecimation(RP_DEC_32); 
        rp_AcqSetTriggerLevel(RP_CH_1, 0.7);
    //rp_AcqSetGain(RP_CH_1, RP_LOW);
    //rp_AcqSetGain(RP_CH_2, RP_HIGH);
    
    double history = 0.0;
    double output = 0.0;
    FILE * fpa;
    FILE * fpb;
    fpa = fopen ("ch1.txt", "w+"); // open the file output is stored
    fpb = fopen ("ch2.txt", "w+");


    while(1){
        // allocate memory
        float *buff_ch1 = (float *)malloc(buff_size * sizeof(float));
        float *buff_ch2 = (float *)malloc(buff_size * sizeof(float));
        rp_AcqStart();

        /* After acquisition is started some time delay is needed in order to acquire fresh samples in to buffer*/
        /* Here we have used time delay of one second but you can calculate exact value taking in to account buffer*/
        /*length and smaling rate*/
        sleep(0.6);
        rp_AcqSetTriggerSrc(RP_TRIG_SRC_CHA_PE);
        rp_acq_trig_state_t state = RP_TRIG_STATE_TRIGGERED;
        //rp_acq_trig_state_t state = RP_TRIG_STATE_WAITING;
        
        // check if red pitaya is properly triggered
        while(1){
                rp_AcqGetTriggerState(&state);
                if(state == RP_TRIG_STATE_TRIGGERED){
                break;
                }
        }
        //rp_AcqGetOldestDataV(RP_CH_1, &buff_size, buff_ch1);
        uint32_t pos = 0;
        rp_AcqGetWritePointerAtTrig(&pos);
        rp_AcqGetDataV2(pos, &buff_size, buff_ch1, buff_ch2);
        clock_t begin = clock();

        //############################user input##############################
        float B_baseline = 0.25;
        float A_baseline = -0.036;  
        float unittime = 4192.0/16384;
        float unitfreq = unittime * 4/2000;
        int trig_time = 100/unittime;
        float center_finetuning = 4340;
        float gain = -0.0001;
        float decay = 0.7;

        //####################################################################
        int counter = 0;
        float numerator = 0.0;
        float denominator = 0.0;
        //float A_sum = 0.0;
        float B_sum = 0.0;
        float sum_temp = 0.0;

        // loop over acquired signal data and store in sum_temp
        for(int i=buff_size_trim+1000;i<buff_size;i++) 
        { 
            sum_temp+=buff_ch2[i]; 
        }
        // B_baseline is average value
        B_baseline =sum_temp/(buff_size-buff_size_trim-1000); 
        // check amount of signals > 0.1
        for(int i = trig_time; i < buff_size_trim; i++){
                        //if (i % 400 == 0) printf("%f  %f\n", buff_ch1[i], buff_ch2[i]);
            if (buff_ch1[i] >0.1) counter++;
                //numerator += i * (buff_ch2[i]-B_baseline);
                //denominator += (buff_ch2[i]-B_baseline);
            // set numerator and denominator and process signal from buff_ch2
            numerator += i * (buff_ch2[i]-B_baseline) * (buff_ch2[i]-B_baseline);
            denominator += (buff_ch2[i]-B_baseline) * (buff_ch2[i]-B_baseline);
            //A_sum +=  buff_ch1[i]-A_baseline;
            B_sum +=  buff_ch2[i]-B_baseline;
            //fprintf(fpa, "%f ", buff_ch1[i]);
            //fprintf(fpb, "%f ", buff_ch2[i]);
            }
        // set center and delta_PC frequencies
        double center = unittime * (numerator/denominator - center_finetuning);
        double delta_PC = unitfreq * (numerator/denominator - center_finetuning);
        float intensity = denominator;
        //fclose(fpa);
        //fclose(fpb);
        bool valid_run = true;
                
        //############################defining a valid run 3db##############################
        /*
        float thresh = 0.32;
        int front = 0;
        for(int i = trig_time; i < trig_time+1000; i++){
            if (buff_ch2[i]>thresh) front += 1;
        }
        int rear = 0;
        for(int i = buff_size_trim-1000; i < buff_size_trim; i++){
            if (buff_ch2[i]>thresh) rear += 1;
        }
        
        if (front < 10 && rear < 10) valid_run = false;
        if (front == 0 || rear == 0) valid_run = false;
        */
        //####################################################################
        //############################defining a valid run pi lowpass##############################
        /*
        float thresh = 0.2;
        int front = 0;
        for(int i = trig_time; i < trig_time+1000; i++){
            if (buff_ch2[i]>thresh) front += 1;
        }
        int rear = 0;
        for(int i = buff_size_trim-1000; i < buff_size_trim; i++){
            if (buff_ch2[i]>thresh) rear += 1;
        }
        
        if (front < 20 && rear < 20) valid_run = false;
        if (front == 0 || rear == 0) valid_run = false;
        */
        //####################################################################
        //############################defining a valid run 2db##############################
        // valid runs are classified to be those over 0.2
        float thresh = 0.2;
        int front = 0;
        for(int i = trig_time; i < trig_time+1000; i++){
            if (buff_ch2[i]>thresh) front += 1;
        }
        int rear = 0;
        for(int i = buff_size_trim-1000; i < buff_size_trim; i++){
            if (buff_ch2[i]>thresh) rear += 1;
        }
        // if number of signals less than 20 in front or rear of buffer
        // or intensity > 1000, than run is a fail
        if (front < 20 && rear < 20) valid_run = false;
        if (front == 0 || rear == 0) valid_run = false;
        if (intensity > 1000) valid_run = false;
        //####################################################################
        
        if (valid_run){
            history = decay * history + center;
            output += history * gain;
        }

        //output = 0;
        if (output>1){
            printf("Hitting upper rail! ");
            output = 1;
        }
        if (output<-1){
            printf("Hitting lower rail! ");
            output = -1;
        }
        rp_GenReset();

        if (output > 0) rp_GenWaveform(RP_CH_1, RP_WAVEFORM_DC);
        else rp_GenWaveform(RP_CH_1, RP_WAVEFORM_DC_NEG);
            rp_GenAmp(RP_CH_1, fabs(output));
            rp_GenOutEnable(RP_CH_1);
        /* #####################################################
        int status = rp_AOpinSetValue(0, 1.0+output);
            if (status != RP_OK) {
            int i = 0;
                    printf("Could not set AO[%i] voltage.\n", i);
            }
        ####################################################### */
        clock_t end = clock();
        double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
        printf("\n time spent = %f ", time_spent);
        //printf("\n");
        //printf("number of nonzero values: %d out of %d \n", counter, buff_size);
        if (valid_run){
            printf("valid run! \n");
            printf("Delta_PC = %f MHz. ", delta_PC);
            //printf("center = %f us.\n", center);
            printf("intensity = %f, ", intensity);
            printf("history = %f. \n", history);
            printf("output = %f. \n", output);    
    
            //for(int i = 0; i < buff_size; i++){
            //  fprintf(fpa, "%f ", buff_ch1[i]);
            //  fprintf(fpb, "%f ", buff_ch2[i]);
            //}
            //fprintf(fpa, "\n");
            //fprintf(fpb, "\n");
        }
        else {
            printf("INVALID run!!! \n");
            if (front==0) printf("front of sequnce has no signal.\n");
            if (rear==0) printf("rear of sequnce has no signal.\n");
            if (front < 20 && rear < 20) printf("cavity most likely unlocked.\n");
            if (intensity > 1000) printf("SPCM saturated.\n");
            //printf("\n");
            //printf("trigger filled %d out of %d! \n \n", counter, buff_size);
                //for(int i = 0; i < buff_size; i++){
                //fprintf(fpa, "%f ", buff_ch1[i]);
                //fprintf(fpb, "%f ", buff_ch2[i]);
                //}
            //fprintf(fpa, "\n");
            //fprintf(fpb, "\n");
        }       
        //for(int i = 0; i < buff_size; i++){
        //  fprintf(fpa, "%f ", buff_ch1[i]);
        //  fprintf(fpb, "%f ", buff_ch2[i]);
        //  }
        //fprintf(fpa, "\n");
        //fprintf(fpb, "\n");
        
            /* Releasing resources */
            free(buff_ch1);
            free(buff_ch2);
    }           
    rp_Release();
    fclose(fpa);
    fclose(fpb);
    return 0;
}









