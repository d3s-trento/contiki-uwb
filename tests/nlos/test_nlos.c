/* Copyright (c) 2021, University of Trento.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \author
 *      Timofei Istomin <tim.ist@gmail.com>
 */

#include "contiki.h"
#include <stdio.h>
#include "dw1000.h"
#include "dw1000-ranging.h"
#include "dw1000-util.h"
#include "dw1000-cir.h"
#include "dw1000-config.h"
/*--------------------------------------------------------------------------*/


#define PRF(str, f) printf(str ": %d.%03d\n", (int)(f), (int)(((f)-(int)(f))*1000))

static void print_nlos(const dw1000_nlos_t *nl) {
  PRF("path_diff", nl->path_diff);
  PRF("pr_nlos", nl->pr_nlos);
  PRF("low_noise", nl->low_noise);
  printf("num_early_peaks: %d\n", nl->num_early_peaks);
  PRF("luep", nl->luep);
  PRF("mc", nl->mc);
  PRF("cl", nl->cl);
}


static void nlos_test() {

  dw1000_cir_sample_t zero_snippet[16] = {0};
  dw1000_nlos_t out;
  
  {
    dwt_rxdiag_t rxdiag = {
      .maxNoise      = 100,
      .firstPathAmp1 = 1000,
      .stdNoise      = 10,   
      .firstPathAmp2 = 500,
      .firstPathAmp3 = 3000,
      .maxGrowthCIR  = 100,
      .rxPreamCount  = 100, 
      .firstPath     = 700*64 + 10,    
      .pacNonsat     = 120,    
      .peakPath      = 701,     
      .peakPathAmp   = 3000  
    };

    dw1000_nlos(&out, &rxdiag, zero_snippet, 16);

    printf("--- Peak path close to the first path, saturation --- \n");
    print_nlos(&out);
  }
  {
    dwt_rxdiag_t rxdiag = {
      .maxNoise      = 100,
      .firstPathAmp1 = 1000,
      .stdNoise      = 10,   
      .firstPathAmp2 = 500,
      .firstPathAmp3 = 3000,
      .maxGrowthCIR  = 100,
      .rxPreamCount  = 100, 
      .firstPath     = 700*64,    
      .pacNonsat     = 120,    
      .peakPath      = 704,     
      .peakPathAmp   = 3000  
    };

    dw1000_nlos(&out, &rxdiag, zero_snippet, 16);

    printf("--- Peak path within 3.3 and 6 later than first path, saturation  --- \n");
    print_nlos(&out);
  }
  {
    dwt_rxdiag_t rxdiag = {
      .maxNoise      = 100,
      .firstPathAmp1 = 1000,
      .stdNoise      = 10,   
      .firstPathAmp2 = 500,
      .firstPathAmp3 = 3000,
      .maxGrowthCIR  = 100,
      .rxPreamCount  = 100, 
      .firstPath     = 700*64,    
      .pacNonsat     = 120,    
      .peakPath      = 707,     
      .peakPathAmp   = 3000  
    };

    dw1000_nlos(&out, &rxdiag, zero_snippet, 16);

    printf("--- Peak path 7 later than first path, saturation  --- \n");
    print_nlos(&out);
  }
  {
    dwt_rxdiag_t rxdiag = {
      .maxNoise      = 100,
      .firstPathAmp1 = 1000,
      .stdNoise      = 10,   
      .firstPathAmp2 = 500,
      .firstPathAmp3 = 3000,
      .maxGrowthCIR  = 100,
      .rxPreamCount  = 100, 
      .firstPath     = 700*64,    
      .pacNonsat     = 120,    
      .peakPath      = 707,     
      .peakPathAmp   = 30000
    };

    dw1000_nlos(&out, &rxdiag, zero_snippet, 16);

    printf("--- Peak path 7 later than first path, no saturation  --- \n");
    print_nlos(&out);
  }
  {
    dwt_rxdiag_t rxdiag = {
      .maxNoise      = 100,
      .firstPathAmp1 = 1000,
      .stdNoise      = 10,   
      .firstPathAmp2 = 500,
      .firstPathAmp3 = 3000,
      .maxGrowthCIR  = 100,
      .rxPreamCount  = 100, 
      .firstPath     = 700*64 + 10,    
      .pacNonsat     = 120,    
      .peakPath      = 701,     
      .peakPathAmp   = 3000  
    };

    uint32_t snippet[16] = {0,10,0,10,0,10,0,10,0,10,0,10,0,10,0,10};
    dw1000_nlos(&out, &rxdiag, (dw1000_cir_sample_t*)snippet, 16);

    printf("--- Peak path close to the first path, saturation, 7 low early peaks --- \n");
    print_nlos(&out);
  }
  {
    dwt_rxdiag_t rxdiag = {
      .maxNoise      = 100,
      .firstPathAmp1 = 1000,
      .stdNoise      = 10,   
      .firstPathAmp2 = 500,
      .firstPathAmp3 = 3000,
      .maxGrowthCIR  = 100,
      .rxPreamCount  = 100, 
      .firstPath     = 700*64 + 10,    
      .pacNonsat     = 120,    
      .peakPath      = 701,     
      .peakPathAmp   = 3000  
    };

    uint32_t snippet[16] = {0,100,0,100,0,100,0,100,0,100,0,100,0,100,0,100};
    dw1000_nlos(&out, &rxdiag, (dw1000_cir_sample_t*)snippet, 16);

    printf("--- Peak path close to the first path, saturation, 7 early peaks --- \n");
    print_nlos(&out);
  }
  {
    dwt_rxdiag_t rxdiag = {
      .maxNoise      = 100,
      .firstPathAmp1 = 1000,
      .stdNoise      = 10,   
      .firstPathAmp2 = 500,
      .firstPathAmp3 = 3000,
      .maxGrowthCIR  = 100,
      .rxPreamCount  = 100, 
      .firstPath     = 700*64 + 10,    
      .pacNonsat     = 120,    
      .peakPath      = 701,     
      .peakPathAmp   = 3000  
    };

    uint32_t snippet[16] = {100,0,0,0,0,0,100,100,0,0,0,0,0,0,0,100};
    dw1000_nlos(&out, &rxdiag, (dw1000_cir_sample_t*)snippet, 16);

    printf("--- Peak path close to the first path, saturation, early quasi peaks --- \n");
    print_nlos(&out);
  }
  {
    dwt_rxdiag_t rxdiag = {
      .maxNoise      = 100,
      .firstPathAmp1 = 1000,
      .stdNoise      = 10,   
      .firstPathAmp2 = 500,
      .firstPathAmp3 = 3000,
      .maxGrowthCIR  = 100,
      .rxPreamCount  = 100, 
      .firstPath     = 700*64 + 10,    
      .pacNonsat     = 120,    
      .peakPath      = 701,     
      .peakPathAmp   = 3000  
    };

    uint32_t snippet[16] = {100,100,0,0,0,0,100,101,100,0,0,0,0,0,100,100};
    dw1000_nlos(&out, &rxdiag, (dw1000_cir_sample_t*)snippet, 16);

    printf("--- Peak path close to the first path, saturation, 1 early peak --- \n");
    print_nlos(&out);
  }

}





PROCESS(test_process, "process");
AUTOSTART_PROCESSES(&test_process);
PROCESS_THREAD(test_process, ev, data)
{
  PROCESS_BEGIN();

  nlos_test();


  PROCESS_END();
}
