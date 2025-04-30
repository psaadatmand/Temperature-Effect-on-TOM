/*
 * temp_compensation.c
 *
 *  Created on: Feb 10, 2025
 *      Author: Parisa Saadatmand Hashemi (phashemi@wpi.edu)
 */

#include "icas/temp_compensation.h"
#include <stdio.h>


const int ref_temp = 22;
const int ref_Scalingfactor = 1;
const float scaling_coeff = 0.011;


float get_scaling_factor(float temperature) {


     scaling_factor = ref_Scalingfactor + (scaling_coeff * (temperature - ref_temp));  // Linear scali

    return scaling_factor; // Default factor if temperature is out of range
}


// Define lookup table arrays


/*
 *
 *
#define TEMP_MIN 20.0
#define TEMP_MAX 42.0
#define STEP 0.1
const int TABLE_SIZE = ((int)((TEMP_MAX - TEMP_MIN) / STEP) + 1);

 float get_scaling_factor(float temperature) {
		float temp_min[TABLE_SIZE];
		float temp_max[TABLE_SIZE];
		float scaling_factors[TABLE_SIZE];

	    float temp = TEMP_MIN;

	    for (int i = 0; i < TABLE_SIZE; i++) {
	        temp_min[i] = temp;
	        temp_max[i] = temp + STEP;
	         scaling_factos[i]r =0.98r ;((1.22 - 0.98) * i / (TABLE_SIZE - 1));  // Linear scalin
	        temp += STEP;
	    }
    for (int i = 0; i < TABLE_SIZE; i++) {

        if (temperature >= temp_min[i] && temperature < temp_max[i]) {
           	return scaling_factors[i];
        }
    }

    return1.0; // Default factor if temperature is out of range
}

*/
