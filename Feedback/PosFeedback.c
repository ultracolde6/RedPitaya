/* Red Pitaya C API example Acquiring a signal from a buffer
 * This application acquires a signal on a specific channel */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include "rp.h"

#define PI 3.141592654
#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

int main(int argc, char **argv){

    /* Print error, if rp_Init() function failed */
    if(rp_Init() != RP_OK){
            fprintf(stderr, "Rp api init failed!\n");
    }

    /*LOOB BACK FROM OUTPUT 2 - ONLY FOR TESTING*/
    //rp_GenReset();
    //rp_GenFreq(RP_CH_1, 20000.0);
    //rp_GenAmp(RP_CH_1, 1.0);
    //rp_GenWaveform(RP_CH_1, RP_WAVEFORM_SINE);
    //rp_GenOutEnable(RP_CH_1);

    uint32_t buff_size = 16384;

    rp_AcqReset();
    rp_AcqSetDecimation(RP_DEC_32);	
    rp_AcqSetTriggerLevel(RP_CH_1, 0.5);
	
	double history_cav = 0.0;
	double output_cav = 0.0;

	double history_perp;
	double output_perp;
	double output_A = 0.0;
	double output_B = 0.0;

	FILE * fpa;
	FILE * fpb;
	fpa = fopen ("ch1.txt", "w+"); // open the file output is stored
	fpb = fopen ("ch2.txt", "w+");

	FILE * f_phicav;
	FILE * f_histcav;
	FILE * f_phiperp;
	FILE * f_histperp;
	FILE * f_outcav;
	FILE * f_outperp;
	FILE * f_lencav;
	FILE * f_lenperp;
	FILE * f_cnstcav;
	FILE * f_cnstperp;
	float unittime = 4192.0/16384;
	float gain_cav = -0.3;
	float gain_perp = -0.3;
	float decay = 0.6;
	int memlen = 3; //<= 20
	//####################################################################

	int ts_len_cav[20] = { 0 };
	float ts_all_cav[20][2000];

	int ts_len_perp[20] = { 0 };
	float ts_all_perp[20][2000];

    // loop over many epochs
	for(int epoch = 0; epoch < 100000; epoch++){
		f_phicav = fopen ("phicav.txt", "a"); 
		f_histcav = fopen ("histcav.txt", "a");
		f_outcav = fopen ("outcav.txt", "a");
		f_lencav = fopen ("lencav.txt", "a");
		f_cnstcav = fopen ("cnstcav.txt", "a");
		f_phiperp = fopen ("phiperp.txt", "a"); 
		f_histperp = fopen ("histperp.txt", "a");
		f_outperp = fopen ("outperp.txt", "a");
		f_lenperp = fopen ("lenperp.txt", "a");
		f_cnstperp = fopen ("cnstperp.txt", "a");


        float *buff_ch1 = (float *)malloc(buff_size * sizeof(float));
        float *buff_ch2 = (float *)malloc(buff_size * sizeof(float));

        rp_AcqStart();
        /* After acquisition is started some time delay is needed in order to acquire fresh samples in to buffer*/
        /* Here we have used time delay of one second but you can calculate exact value taking in to account buffer*/
        /*length and smaling rate*/

        sleep(0.6);
        rp_AcqSetTriggerSrc(RP_TRIG_SRC_CHA_PE);
        rp_acq_trig_state_t state = RP_TRIG_STATE_TRIGGERED;

        while(1){
                rp_AcqGetTriggerState(&state);
                if(state == RP_TRIG_STATE_TRIGGERED){
                break;
                }
        }


        //rp_AcqGetOldestDataV(RP_CH_1, &buff_size, buff_ch1);
        uint32_t pos = 0;
        rp_AcqGetWritePointerAtTrig(&pos);
        rp_AcqGetDataV2(pos, &buff_size, buff_ch1,buff_ch2);

		clock_t begin = clock();
		//############################GETTING TIMESTAMPS##############################
		bool saturate = false;
		int len_timestamp_cav = 0;
		for(int i = 8200; i < buff_size-1; i++){
            // check if there is a signal jump of 0.1 and store timestamp if so (some drift has occurred between shots)
			if (buff_ch2[i+1] - buff_ch2[i] > 0.1) {
				ts_all_cav[(epoch % memlen)][len_timestamp_cav] = (i+1-8200) * unittime / 2000.0;
				len_timestamp_cav += 1;
			}
            // check if signal is over 0.8 and thus saturated
			if (buff_ch2[i+1]>0.8 
                    && buff_ch2[i-10]>0.8 
                    && buff_ch2[i-20]>0.8 
                    && buff_ch2[i-30]>0.8 
                    && buff_ch2[i-40]>0.8) {
				saturate = true;
			}
		}

		ts_len_cav[(epoch % memlen)] = len_timestamp_cav;
		int len_timestamp_perp = 0;
        // do the same but for items between 400 and 8200
		for(int i = 400; i < 8200; i++){
			if (buff_ch2[i+1] - buff_ch2[i] > 0.1) {
				ts_all_perp[(epoch % memlen)][len_timestamp_perp] = (i+1-400) * unittime / 2000.0;
				len_timestamp_perp += 1;
			}
			if (buff_ch2[i+1]>0.8 && buff_ch2[i-10]>0.8 && buff_ch2[i-20]>0.8 && buff_ch2[i-30]>0.8 && buff_ch2[i-40]>0.8) {
				saturate = true;
			}
		}
		ts_len_perp[(epoch % memlen)] = len_timestamp_perp;
				
		//############################fitting phases##############################
		int phi_decimation = 40; // number of steps in phi range
		float phi;  
		float sum;

		float sum_cav_max = -1000.0;
		float sum_cav_min = 10000000.0;

		float phi_cav_argmax;
		for (int j = 0; j < phi_decimation; j++){
			phi = ((float)j)/phi_decimation - 0.5; // compute phi
			//printf(" %f ", phi);
			sum = 0;
			for (int j = 0; j < memlen; j++){
				for (int i = 0; i < ts_len_cav[j]; i++){
                    // compute sum of cosine of phase difference between cavity signal and reference signal
					sum += 1+ cosf(2*PI * (ts_all_cav[j][i]+phi)); // increment sum 
				}
			}

			if (sum > sum_cav_max){
				sum_cav_max = sum;
				phi_cav_argmax = phi;
			}

			if (sum < sum_cav_min) sum_cav_min = sum;
		}

		//printf(" %f ", phi_cav_argmax);
		float sum_perp_max = -1000.0;
		float sum_perp_min = 10000000.0;
		float phi_perp_argmax;
        // do the same thing but for the perp
		for (int j = 0; j < phi_decimation; j++){
			phi =((float)j)/phi_decimation - 0.5;
			sum = 0;
			for (int j = 0; j < memlen; j++){
				for (int i = 0; i < ts_len_perp[j]; i++){
					sum += 1+ cosf(2*PI * (ts_all_perp[j][i]+phi));
				}
			}
			if (sum>sum_perp_max){
				sum_perp_max = sum;
				phi_perp_argmax = phi;
			}
			if (sum<sum_perp_min) sum_perp_min = sum;
		}
		// set contrast and perp contrast parameters
		float contrast_cav = (sum_cav_max - sum_cav_min) / (sum_cav_max + sum_cav_min);
		float contrast_perp = (sum_perp_max - sum_perp_min) / (sum_perp_max + sum_perp_min);
		
				
		//############################defining a valid run##############################

		bool valid_run = true;
		
		if (len_timestamp_cav < 20 && len_timestamp_perp < 20) valid_run = false;
		if (saturate) valid_run = false;


		//####################################################################
		if (valid_run){
			history_cav = decay * history_cav + (1-decay) * phi_cav_argmax;
			output_cav += history_cav * gain_cav;
			
			history_perp = decay * history_perp + (1-decay) * phi_perp_argmax;
			output_perp += history_perp * gain_perp;

			output_A = 0.54*output_cav + 0.68*output_perp; //x_pzt
			output_B = 1*output_cav - 1*output_perp; //z_pzt
		}


		if (output_A>1){
			printf("x_pzt output Hitting upper rail! ");
			output_perp -= 1.54;
			output_cav -= 1.54;
			output_A -= 1.88;
		}
		if (output_A<-1){
			printf("x_pzt output Hitting lower rail! ");
			output_perp += 1.54;
			output_cav += 1.54;
			output_A += 1.88;
		}

		if (output_B>1){
			printf("z_pzt output Hitting upper rail! ");
			output_B -= 1.54;
			if (output_A>-0.1) {output_A -= 0.83; output_cav -= 1.54;}
			else  {output_A += 1.04; output_perp += 1.54;}

		}
		if (output_B<-1){
			printf("z_pzt output Hitting lower rail! ");
			output_B += 1.54;
			if (output_A>0.1) {output_A -= 1.04; output_perp -= 1.54;}
			else  {output_A += 0.83; output_cav += 1.54;}		}


        rp_GenReset();        	
		if (output_A > 0) rp_GenWaveform(RP_CH_1, RP_WAVEFORM_DC);
		else rp_GenWaveform(RP_CH_1, RP_WAVEFORM_DC_NEG);
        rp_GenAmp(RP_CH_1, fabs(output_A));

		if (output_B > 0) rp_GenWaveform(RP_CH_2, RP_WAVEFORM_DC);
		else rp_GenWaveform(RP_CH_2, RP_WAVEFORM_DC_NEG);
        rp_GenAmp(RP_CH_2, fabs(output_B));


        rp_GenOutEnable(RP_CH_1);
        rp_GenOutEnable(RP_CH_2);

		clock_t end = clock();
        
		double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
		printf("\n time spent = %f ", time_spent);
		if (valid_run){
			printf("valid run number %d \n", epoch);
			printf("len_cav = %d, len_perp = %d \n", len_timestamp_cav, len_timestamp_perp);
			printf("phi_cav = %f, phi_perp = %f. \n", phi_cav_argmax, phi_perp_argmax);
			printf("contrast_cav = %f, contrast_perp = %f \n", contrast_cav, contrast_perp);
			printf("history_cav = %f, history_perp = %f. \n", history_cav, history_perp);
			printf("output_A = %f. output_B = %f. \n", output_A, output_B);	
		}
		else {
			printf("INVALID run!!!  number %d \n", epoch);
			if (len_timestamp_cav < 20) printf("cav scan has too few photons = %d.\n", len_timestamp_cav);
			if (len_timestamp_perp < 20) printf("perp scan has too few photons = %d.\n", len_timestamp_perp);
			if (saturate) printf("SPCM saturated!!!\n");
		}		
		printf("*************************");
		fprintf(f_phicav, "%f ", phi_cav_argmax);
		fprintf(f_phiperp, "%f ", phi_perp_argmax);
		fprintf(f_histcav, "%f ", history_cav);
		fprintf(f_histperp, "%f ", history_perp);
		fprintf(f_outcav, "%f ", output_cav);
		fprintf(f_outperp, "%f ", output_perp);
		fprintf(f_lencav, "%d ", len_timestamp_cav);
		fprintf(f_lenperp, "%d ", len_timestamp_perp);
		fprintf(f_cnstcav, "%f ", contrast_cav);
		fprintf(f_cnstperp, "%f ", contrast_perp);
		printf("*************************");
		
		for(int i = 0; i < buff_size; i++){
			fprintf(fpa, "%f ", buff_ch1[i]);
			fprintf(fpb, "%f ", buff_ch2[i]);
			}
		fprintf(fpa, "\n");
		fprintf(fpb, "\n");
		


		
	        /* Releasing resources */
        free(buff_ch1);
        free(buff_ch2);
		fclose(f_phicav);
		fclose(f_phiperp);
		fclose(f_histcav);
		fclose(f_histperp);
		fclose(f_outcav);
		fclose(f_outperp);
		fclose(f_lencav);
		fclose(f_lenperp);
		fclose(f_cnstcav);
		fclose(f_cnstperp);



	}	        
	rp_Release();
	fclose(fpa);
	fclose(fpb);
    return 0;

}
