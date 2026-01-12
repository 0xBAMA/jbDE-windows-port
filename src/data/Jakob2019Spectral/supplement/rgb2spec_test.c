#include "rgb2spec.h"
#include <stdio.h>

int main(int argc, char **argv) {
    RGB2Spec *model = rgb2spec_load("tables/srgb.coeff");

    float rgb[3] = {.8f, .2f, .3f}, coeff[3];

    rgb2spec_fetch(model, rgb, coeff);
    printf("fetch(): %f %f %f\n", coeff[0], coeff[1], coeff[2]);
    printf("eval():\n");

    float lambda = 532;
    {
        float result = rgb2spec_eval_precise(coeff, lambda);
        printf("  %f\n", result);
    }
    {
        float result = rgb2spec_eval_fast(coeff, lambda);
        printf("  %f\n", result);
    }

    rgb2spec_free(model);
}
