/*
 * Copyright (c) 2015, Nordic Semiconductor
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/**
 * \addtogroup nrf52832
 * @{
 *
 * \addtogroup nrf52832-dev Device drivers
 * @{
 *
 * \addtogroup nrf52832-rng Hardware random number generator
 * @{
 *
 * \file
 *         Random number generator routines exploiting the nRF52 hardware
 *         capabilities.
 *
 *         This file overrides core/lib/random.c.
 *
 * \author
 *         Wojciech Bober <wojciech.bober@nordicsemi.no>
 */
#include <stddef.h>
#include <nrfx_rng.h>
#include <nrf_queue.h>
#ifdef SOFTDEVICE_PRESENT
#include "nrf_sdh.h"
#endif
#include "app_error.h"

/*---------------------------------------------------------------------------*/
static const nrfx_rng_config_t default_config = NRFX_RNG_DEFAULT_CONFIG;
#ifdef SOFTDEVICE_PRESENT
    #define SD_RAND_POOL_SIZE           (64)

    STATIC_ASSERT(RNG_CONFIG_POOL_SIZE == SD_RAND_POOL_SIZE);

    #define NRF_DRV_RNG_LOCK()          CRITICAL_REGION_ENTER()
    #define NRF_DRV_RNG_RELEASE()       CRITICAL_REGION_EXIT()
    #define NRF_DRV_RNG_SD_IS_ENABLED() nrf_sdh_is_enabled()
#else
    #define NRF_DRV_RNG_LOCK()          do { } while (0)
    #define NRF_DRV_RNG_RELEASE()       do { } while (0)
    #define NRF_DRV_RNG_SD_IS_ENABLED() false
#endif // SOFTDEVICE_PRESENT

NRF_QUEUE_DEF(uint8_t, m_rand_pool, RNG_CONFIG_POOL_SIZE, NRF_QUEUE_MODE_OVERFLOW);

static void nrfx_rng_handler(uint8_t rng_val)
{
  NRF_DRV_RNG_LOCK();
  if (!NRF_DRV_RNG_SD_IS_ENABLED())
    {
      UNUSED_RETURN_VALUE(nrf_queue_push(&m_rand_pool, &rng_val));

      if (nrf_queue_is_full(&m_rand_pool))
        {
	  nrfx_rng_stop();
        }
    }
  NRF_DRV_RNG_RELEASE();
}

/* function took from legacy layer of nRF532 SDK 15.3.0 to bound dependency to this functions*/
void nrf_drv_rng_bytes_available(uint8_t * p_bytes_available)
{
#ifdef SOFTDEVICE_PRESENT
    if (NRF_DRV_RNG_SD_IS_ENABLED())
    {
        if (NRF_SUCCESS == sd_rand_application_bytes_available_get(p_bytes_available))
        {
            return;
        }
    }
#endif // SOFTDEVICE_PRESENT

    *p_bytes_available  = nrf_queue_utilization_get(&m_rand_pool);
}

ret_code_t nrf_drv_rng_rand(uint8_t * p_buff, uint8_t length)
{
    ret_code_t err_code = NRF_SUCCESS;

#ifdef SOFTDEVICE_PRESENT
    do {
        bool sd_is_enabled;
        NRF_DRV_RNG_LOCK();
        sd_is_enabled = NRF_DRV_RNG_SD_IS_ENABLED();
        if (!sd_is_enabled)
#endif // SOFTDEVICE_PRESENT
        {
            err_code = nrf_queue_read(&m_rand_pool, p_buff, (uint32_t)length);
            nrfx_rng_start();
        }
#ifdef SOFTDEVICE_PRESENT
        NRF_DRV_RNG_RELEASE();

        if (sd_is_enabled)
        {
            err_code = sd_rand_application_vector_get(p_buff, length);
            if (err_code == NRF_ERROR_SOC_RAND_NOT_ENOUGH_VALUES)
            {
                err_code = NRF_ERROR_NOT_FOUND;
            }
        }
    } while (err_code == NRF_ERROR_SOFTDEVICE_NOT_ENABLED);
#endif // SOFTDEVICE_PRESENT
    ASSERT((err_code == NRF_SUCCESS) || (err_code == NRF_ERROR_NOT_FOUND));

    return err_code;
}

/**
 * \brief Generates a new random number using the nRF52 RNG.
 * \return a random number.
 */
unsigned short
random_rand(void)
{
  unsigned short value = 42;
  uint8_t available;
  ret_code_t err_code;

  do {
    nrf_drv_rng_bytes_available(&available);
  } while (available < sizeof(value));

  err_code = nrf_drv_rng_rand((uint8_t *)&value, sizeof(value));
  APP_ERROR_CHECK(err_code);

  return value;
}
/*---------------------------------------------------------------------------*/
/**
 * \brief Initialize the nRF52 random number generator.
 * \param seed Ignored. It's here because the function prototype is in core.
 *
 */
void
random_init(unsigned short seed)
{
  ret_code_t err_code = NRF_SUCCESS;
  NRF_DRV_RNG_LOCK();

  if (!NRF_DRV_RNG_SD_IS_ENABLED())
    {
      err_code = nrfx_rng_init(&default_config, nrfx_rng_handler);
      if (err_code == NRF_SUCCESS)
	nrfx_rng_start();
    }

  NRF_DRV_RNG_RELEASE();

  APP_ERROR_CHECK(err_code);
}
/**
 * @}
 * @}
 * @}
 */
