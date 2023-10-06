/* Red Pitaya C API example Acquiring a signal from a buffer
 * This application acquires a signal on a specific channel */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include "rp.h"

#define PI 3.141592654

//function prototypes

void setRP();
bool acquireSignal(float **buff_ch1, float **buff_ch2, uint32_t *buff_size);
bool acquireTimestamp(float *buff_ch2, 
					int buff_size, 
					float ts_all_cav[][2000], 
					float ts_all_perp[][2000], 
					int ts_len_cav[], 
					int ts_len_perp[], 
					int epoch, 
					int memlen, 
					float unittime,
					int len_timestamp_cav,
					int len_timestamp_perp);
struct phaseContrast;
struct phaseContrast computeContrast(float ts_all_cav[][2000],
					float ts_all_perp[][2000], 
					int ts_len_cav[], 
					int ts_len_perp[], 
					int memlen, 
					int phi_decimation);

bool computeValidity(int len_timestamp_cav, 
						int len_timestamp_perp, 
						bool saturate);
struct outputValues;
struct outputValues calculateOutputValues(double history_cav, 
										double history_perp, 
										double decay, 
										double gain_cav, 
										double gain_perp, 
										double phi_cav_argmax, 
										double phi_perp_argmax, 
										bool valid_run);

void setRP(){
	rp_AcqReset();
    rp_AcqSetDecimation(RP_DEC_32);	
    rp_AcqSetTriggerLevel(RP_CH_1, 0.5);
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

bool acquireTimestamp(float *buff_ch2, 
					int buff_size, 
					float ts_all_cav[][2000], 
					float ts_all_perp[][2000], 
					int ts_len_cav[], 
					int ts_len_perp[], 
					int epoch, 
					int memlen, 
					float unittime,
					int len_timestamp_cav,
					int len_timestamp_perp) {
    bool saturate = false;
    for (int i = 8200; i < buff_size - 1; i++) {
        // Check if there is a signal jump of 0.1 and store timestamp if so (some drift has occurred between shots)
        if (buff_ch2[i + 1] - buff_ch2[i] > 0.1) {
            ts_all_cav[(epoch % memlen)][len_timestamp_cav] = (i + 1 - 8200) * unittime / 2000.0;
            len_timestamp_cav += 1;
        }
        // Check if signal is over 0.8 and thus saturated
        if (buff_ch2[i + 1] > 0.8 &&
            buff_ch2[i - 10] > 0.8 &&
            buff_ch2[i - 20] > 0.8 &&
            buff_ch2[i - 30] > 0.8 &&
            buff_ch2[i - 40] > 0.8) {
            saturate = true;
        }
    }
    ts_len_cav[(epoch % memlen)] = len_timestamp_cav;
    // Do the same but for items between 400 and 8200
    for (int i = 400; i < 8200; i++) {
        if (buff_ch2[i + 1] - buff_ch2[i] > 0.1) {
            ts_all_perp[(epoch % memlen)][len_timestamp_perp] = (i + 1 - 400) * unittime / 2000.0;
            len_timestamp_perp += 1;
        }
        if (buff_ch2[i + 1] > 0.8 &&
            buff_ch2[i - 10] > 0.8 &&
            buff_ch2[i - 20] > 0.8 &&
            buff_ch2[i - 30] > 0.8 &&
            buff_ch2[i - 40] > 0.8) {
            saturate = true;
        }
    }
    ts_len_perp[(epoch % memlen)] = len_timestamp_perp;
    return saturate;
}
struct phaseContrast{
	float phi_cav_argmax;
	float phi_perp_argmax;
	float contrast_cav;
	float contrast_perp;
}
// calculate contrast values
struct phaseContrast computeContrast(float ts_all_cav[][2000], 
					float ts_all_perp[][2000], 
					int ts_len_cav[], 
					int ts_len_perp[], 
					int memlen, 
					int phi_decimation) {
    float phi;
    float sum;
	float phi_cav_argmax;
	float phi_perp_argmax;
	float contrast_cav;
	float contrast_perp;
	struct phaseContrast result;
    float sum_cav_max = -1000.0;
    float sum_cav_min = 10000000.0;
    for (int j = 0; j < phi_decimation; j++) {
        phi = ((float)j) / phi_decimation - 0.5;
        sum = 0;
		// sum of the cosines of phase difference between cavity signal and reference signal
        for (int j = 0; j < memlen; j++) {
            for (int i = 0; i < ts_len_cav[j]; i++) {
                sum += 1 + cosf(2 * PI * (ts_all_cav[j][i] + phi));
            }
        }
		// store signal if its current sum is greater than max sum
        if (sum > sum_cav_max) {
            sum_cav_max = sum;
            phi_cav_argmax = phi;
        }
		// do the same for min
        if (sum < sum_cav_min) {
            sum_cav_min = sum;
        }
    }
	// store max value in struct
	phaseContrast.phi_cav_argmax = phi_cav_argmax;

	// repeat for perp
    float sum_perp_max = -1000.0;
    float sum_perp_min = 10000000.0;
    for (int j = 0; j < phi_decimation; j++) {
        phi = ((float)j) / phi_decimation - 0.5;
        sum = 0;
        for (int j = 0; j < memlen; j++) {
            for (int i = 0; i < ts_len_perp[j]; i++) {
                sum += 1 + cosf(2 * PI * (ts_all_perp[j][i] + phi));
            }
        }
        if (sum > sum_perp_max) {
            sum_perp_max = sum;
            phi_perp_argmax = phi;
        }
        if (sum < sum_perp_min) {
            sum_perp_min = sum;
        }
    }
	// store it
	result.phi_perp_argmax = phi_perp_argmax;
	// compute contrast values
    result.contrast_cav = (sum_cav_max - sum_cav_min) / (sum_cav_max + sum_cav_min);
	result.contrast_perp = (sum_perp_max - sum_perp_min) / (sum_perp_max + sum_perp_min);

	return result;
}

bool computeValidity(int len_timestamp_cav, 
						int len_timestamp_perp, 
						bool saturate){
	bool valid_run = true;
	if (len_timestamp_cav < 20 && len_timestamp_perp < 20) valid_run = false;
	if (saturate) valid_run = false;
	return valid_run;
}

struct outputValues{
	double output_cav;
	double output_perp;
	double output_A;
	double output_B;
}

struct outputValues calculateOutputValues(double history_cav, 
										double history_perp, 
										double decay, 
										double gain_cav, 
										double gain_perp, 
										double phi_cav_argmax, 
										double phi_perp_argmax, 
										bool valid_run) {
	double output_cav = 0.0;
	double output_perp = 0.0;
	double output_A = 0.0;
	double output_B = 0.0

	// Calculate the history and output values if the run is valid
    if (valid_run){
        history_cav = decay * history_cav + (1-decay) * phi_cav_argmax;
        output_cav += history_cav * gain_cav;

        history_perp = decay * history_perp + (1-decay) * phi_perp_argmax;
        output_perp += history_perp * gain_perp;

        output_A = 0.54*output_cav + 0.68*output_perp; //x_pzt
        output_B = 1*output_cav - 1*output_perp; //z_pzt
    }
	// Check if the output values are hitting the upper or lower rails, and adjust them accordingly
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
        else  {output_A += 0.83; output_cav += 1.54;}        }
	
	// Set the output values for the two channels using the rp_GenWaveform() and rp_GenAmp() functions,
    // and enable the output using the rp_GenOutEnable() function
    rp_GenReset();            
    if (output_A > 0) rp_GenWaveform(RP_CH_1, RP_WAVEFORM_DC);
    else rp_GenWaveform(RP_CH_1, RP_WAVEFORM_DC_NEG);
    rp_GenAmp(RP_CH_1, fabs(output_A));

    if (output_B > 0) rp_GenWaveform(RP_CH_2, RP_WAVEFORM_DC);
    else rp_GenWaveform(RP_CH_2, RP_WAVEFORM_DC_NEG);
    rp_GenAmp(RP_CH_2, fabs(output_B));

    rp_GenOutEnable(RP_CH_1);
    rp_GenOutEnable(RP_CH_2);

	struct outputValues outputs;
	outputs.output_A = output_A;
	outputs.output_B = output_B;
	outputs.output_cav = output_cav;
	outputs.output_perp = output_perp;

	return outputs;
}

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
	double history_cav = 0.0;
	double output_cav = 0.0;
	int phi_decimation = 40;
	int len_timestamp_cav = 0;
	int len_timestamp_perp = 0;

	double history_perp;
	double output_perp;
	double output_A = 0.0;
	double output_B = 0.0;
	struct phaseContrast phaseContrast;
	struct outputValues outputValues;

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

	int ts_len_cav[20] = { 0 };
	float ts_all_cav[20][2000];

	int ts_len_perp[20] = { 0 };
	float ts_all_perp[20][2000];
	// set the RP settings
	setRP();

    // loop over many epochs and open up files to populate
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

		// allocate memory
        float *buff_ch1 = (float *)malloc(buff_size * sizeof(float));
        float *buff_ch2 = (float *)malloc(buff_size * sizeof(float));

		// acquire the signal
		while (1) {
        if (!acquireSignal(&buff_ch1, &buff_ch2, &buff_size)) {
            break;
        }

		clock_t begin = clock();
		// get timestamps and determine if anything is saturated
		bool saturate = acquireTimestamp(buff_ch2, 
											buff_size, 
											ts_all_cav, 
											ts_all_perp, 
											ts_len_cav, 
											ts_len_perp, 
											epoch, 
											memlen, 
											unittime,
											len_timestamp_cav,
											len_timestamp_perp);		
		// fit the phases
		phaseContrast = computeContrast(ts_all_cav, 
										ts_all_perp, 
										ts_len_cav, 
										ts_len_perp, 
										memlen, 
										phi_decimation);
				
		// check if run is valid
		bool valid_run = computeValidity(len_timestamp_cav,
											len_timestamp_perp, 
											saturate);
		// perform feedback and checks		
		outputValues = calculateOutputValues(history_cav, 
												history_perp, 
												decay, 
												gain_cav, 
												gain_perp, 
												phaseContrast.phi_cav_argmax, 
												phaseContrast.phi_perp_argmax, 
												valid_run);
		clock_t end = clock();
        
		// provide run info
		double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
		printf("\n time spent = %f ", time_spent);
		if (valid_run){
			printf("valid run number %d \n", epoch);
			printf("len_cav = %d, len_perp = %d \n", len_timestamp_cav, len_timestamp_perp);
			printf("phi_cav = %f, phi_perp = %f. \n", phaseContrast.phi_cav_argmax, phaseContrast.phi_perp_argmax);
			printf("contrast_cav = %f, contrast_perp = %f \n", phaseContrast.contrast_cav, phaseContrast.contrast_perp);
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
		fprintf(f_phicav, "%f ", phaseContrast.phi_cav_argmax);
		fprintf(f_phiperp, "%f ", phaseContrast.phi_perp_argmax);
		fprintf(f_histcav, "%f ", history_cav);
		fprintf(f_histperp, "%f ", history_perp);
		fprintf(f_outcav, "%f ", output_cav);
		fprintf(f_outperp, "%f ", output_perp);
		fprintf(f_lencav, "%d ", len_timestamp_cav);
		fprintf(f_lenperp, "%d ", len_timestamp_perp);
		fprintf(f_cnstcav, "%f ", phaseContrast.contrast_cav);
		fprintf(f_cnstperp, "%f ", phaseContrast.contrast_perp);
		printf("*************************");
		
		// populate files
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
