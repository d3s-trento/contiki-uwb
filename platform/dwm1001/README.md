# DecaWave DWM1001-DEV Contiki Port 

## Changing default nRF52832 stack configuration
```
#ifeq ($(CONTIKI_WITH_RIME),1)
#  $(error Rime stack is not supported!)
#endif

#ifneq ($(CONTIKI_WITH_IPV6),1)
#  $(error Only IPv6 stack is supported!)
#endif
```
