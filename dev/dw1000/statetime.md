# Statetime

Statetime is a module for the DW1000 radio used to compute dwell times,
i.e. the time spent in each state,
using the accurate timestamps reported by the radio combined with
estimates on expected frame durations.

Statetime is built replicating a subset of the DW1000 radio state machine
and provides mechanism to infer dwell times also when complex
scheduling operations are used, like delayed transmission or
reception after transmission.


## Operations

Statetime provides tracing by code instrumentation.
Statetime functions attempt to match the full set of features
of the DW1000 radios.

To monitor a given radio activity, the Statetime function that
best matches the given operation should be issued after the
radio correctly schedules the desired action.

Statetime functions can be grouped in three classes based
on the expected layer in which they are used.

### Application Layer

Functions used to reset, start and stop Statetime and
collect the corresponding logs.

* `dw1000_statetime_context_init()`
  resets the Statetime counters.

* `dw1000_statetime_start()`
    instructs Statetime to start tracing radio operations.

* `dw1000_statetime_stop()`
    instructs Statetime to stop tracing radio operations.

* `dw1000_statetime_print()`
    prints Statetime counters directly to the standard output.

* `dw1000_statetime_log(dw1000_statetime_log_t* entry)`
    stores Statetime counters to a Statetime log entry.
    Useful when applications have complex logs structures.


### Scheduling Functions

Functions used to instrument the code next to the
corresponding radio API routines. These functions
should be issued iff the radio function was
successfully scheduled (returned `DWT_SUCCESS`).

Note that the following functions consider only delayed operations.
In case of immediate actions the user can still make use of Statetime
by setting an estimated time at which the operation is scheduled,
considering the radio switching time and so on.

In order to avoid unexpected outcomes when using Statetime, be sure
to have the full control over the radio API, in particular when
the default behaviour cannot be traced by Statetime.
Namely, when scheduling receptions, be sure to set the `DWT_IDLE_ON_DLY_ERR`
to make the radio switch back to IDLE in case of scheduling failures.


* `dw1000_statetime_schedule_tx(const uint32_t schedule_tx_32hi)`
    used when scheduling a delayed transmission. The argument is the time at which the RMarker transits the antenna, not considering the antenna delay.
    Statetime does not take into account the antenna delay in any way.

* `dw1000_statetime_schedule_txrx(const uint32_t schedule_tx_32hi, const uint32_t rx_delay_uus)`
    used when scheduling a delayed tranmission followed by a reception. The rx delay
    is the time in UWB us at which the radio will switch to RX once the transmission
    ends.

* `dw1000_statetime_schedule_rx(const uint32_t schedule_rx_32hi)`
    used when scheduling a delayed reception. The argument provides the
    timestamp at which the radio is able to listen for the preamble.


### Event functions

These functions are used to instrument callbacks in response of
radio events.

* `dw1000_statetime_after_rx(const uint32_t sfd_rx_32hi, const uint16_t framelength)`
    used after a frame has been correctly received. This function requires
    both the `SYS_STATUS_LDEDONE` and `SYS_STATUS_RXFCG` flagged to work properly, since
    the former provides the SFD timestamp and the latter signals the successfull Rx.

* `dw1000_statetime_after_rxerr(const uint32_t now_32hi)`
    used when either a reception error or a reception timeout occurs. The argument
    is the current timestamp provided by the radio, which must be later then
    the previously scheduled RX operation and later than time in which the error does
    occur.

    At the moment Statetime assumes that the time spent from the scheduling of the Rx operation
    up to the moment passed to the function to be in **preamble hunting**, regardless of the
    type of the error (or timeout).

* `dw1000_statetime_after_tx(const uint32_t sfd_tx_32hi, const uint16_t framelength)`
    used after a frame has been successfully transmitted and the corresponding SFD time
    is available.

* `dw1000_statetime_abort(const uint32_t now_32hi)`
    used when interrupting any pending radio operation, such as after a `dwt_forcetrxoff`.
    Statetime does its best to trace this condition based on the scheduled operation and
    whether the radio operation was already performing at the calling time.

    When aborting an ongoing Rx operation the elapsed time is considered to be spent
    in preamble hunting.

    When aborting an ongoing Tx operation, the elapsed time is considered to be spent
    transmitting the preamble.

    <!-- TODO: maybe based on the SYS_STATUS register we can determine if the preamble TX had completed. -->


## Statetime Internals

Statetime is built based on a simplified state machine of the DW1000 radio and
considering the scheduling features provided. According to Statetime the radio can be
in one of the following states:

* `DW1000_IDLE`
* `DW1000_SCHEDULED_TX`
* `DW1000_SCHEDULED_RX`

When issuing a `dw1000_statetime_start`, Statetime begins monitoring radio operations.
The state is set to IDLE and a context variable `is_restarted` signals that Statetime
just began its activities.
Scheduling functions make the state transit to `DW1000_SCHEDULED_TX` and `DW1000_SCHEDULED_RX`
states, respectively. When scheduling, the context variable `schedule_32hi` stores
the schedule time of the corresponding operation, being the time at which the RMarker transits
the antenna in case of Tx or the time in which the radio can start hunting for the
preamble in case of Rx.

Of particular interest is the `dw1000_statetime_schedule_txrx` function, which
sets the state to `DW1000_SCHEDULED_TX` and store the scheduling options for the later
Rx. Once Tx is completed and the `dw1000_statetime_after_tx` is issued, the rx options
previously stored are used to compute the expected RX schedule based on the end of the frame Tx,
and the Rx delay set when scheduling the Tx-Rx operation.

On the occurrence of one of the radio events the corresponding statetime function is called
(`dw1000_statetime_after_rx`, `dw1000_statetime_after_tx`, `dw1000_statetime_after_rxerr`),
increasing counters related to the individual components of the frame and states of the
radio:

* idle time, tx preamble, tx data, rx hunt, rx preamble and rx data.


Statetime relies on estimates of the preamble and phy payload durations to update
the reference timestamps used to increase Statetime counters:

* with the term preamble time *P*, Statetime denotes the time required to
    Tx or Rx the preamble and the SFD, until the RMarker.

* With the term payload time *D*, Statetime denotes the time required to
    Tx or Rx the PHR and the physical layer payload, comprehensive of the 2
    bytes of CRC.

In what follows, we describe the two main Statetime functions issued in response
to radio events. Other event functions act in a similar fashion.

### After Tx

Statetime exploits the SFD timestamp returned by the radio and updates
the time references as follows:

1. computes the time *IdleSFD* between the last idle instant and the SFD,
2. increases the idle time counter with the time elapsed between last IDLE
   and the beginning of the preamble, subtracting the preamble time *P* from
   *IdleSFD* previously computed,

3. increases the tx preamble and tx data counters by *P* and *D*, respectively,

4. updates the last idle instant by adding the PHY payload time *D* to
   the SFD timestamp.

5. the state is set to IDLE

In case a Tx-Rx operation was scheduled, the internal Statetime context stored
the rx delay to be used to turn the radio on after the transmission finished.
In which case:

6. the new schedule is computed by adding the Rx delay to the last idle time previously
     updated,

7. the saved options for delayed reception are reset and the state is set
     to `DW1000_SCHEDULED_RX`.


### After Rx

Statetime exploits the SFD timestamp similarly to the after-Tx scenario, but relies
also on the `schedule_32hi` variable which defines the time in which the radio started
hunting for the preamble.

1. The time `ScheduleSFD` elapsed betwen the time the radio went to Rx and the actual Rx SFD
    is computed,

2. a) If *ScheduleSFD* is greater than or equal to the preamble time, the time spent in preamble hunting
    is computed subtracting *P* from *ScheduleSFD* and the it is assumed that the radio
    has listened for the entire preamble duration *P*;

2. b) In case *ScheduleSFD* is less than the preamble time, *ScheduleSFD* is considered
    to be the actual preamble time and the radio considers that no preamble hunting
    has been performed (the radio turned on and saw directly the preamble).

3. the idle time counter is updated with the time elapsed between the last idle moment and
   the schedule timestamp (which is the time in which the radio switched to Rx in this case).

4. the rx preamble and rx data counters are updated with *P* and *D*, respectively;

5. the last idle moment is updated by adding the data payload time *D* to the Rx SFD
   of the current reception;

6. Finally, the state is set back to IDLE.


### Considerations on the first and last operation

When issuing `dw1000_statetime_start`, the user instructs Statetime to trace
any radio operation that follows, until requested to stop with the
`dw1000_statetime_stop`.

Statetime computes time differences when explicitly instructed to do so, i.e.:

1. upon the occurrence of an event;
2. when the user issues the `dw1000_statetime_abort` function
    to consider dwell times before an abnormal interruption of
    a radio operation.

These functions act as **control points** for Statetime.


The `dw1000_statetime_start` and `dw1000_statetime_stop` functions are not
control points. Statetime therefore does not timestamp the moment they
are issued and does not consider the time between their invocation and
the next (for `dw1000_statetime_start`) or previous (for `dw1000_statetime_stop`)
control point, respectively.

This means that when the first event is triggered, Statetime has no reference
idle timestamp to be used when computing time deltas.
To address this issue, when the first Statetime event function is invoked,
Statetime uses either the schedule or the SFD timestamp
of the operation to set the initial idle moment as:

1. *sfd_tx_32hi* - *P* - *S* in case of Tx
2. *schedule_32hi* in case of Rx

where *S* is a slack time of 8 *ns* which models variations in the SFD timestamp
given by the 8 *ns* precision when scheduling.


When considering the last operation, the user can issue a `dw1000_statetime_abort`
call before the stop function, to consider the time elapsed between the last
tracked operation and the moment Statetime is instructed to stop.

