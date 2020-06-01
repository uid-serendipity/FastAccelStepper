#include "FastAccelStepper.h"

#define stepPinStepperA   9  /* OC1A */
#define stepPinStepperB   10 /* OC1B */

// Here are the global variables to interface with the interrupts

uint8_t fas_ledPin = 255;   // 255 if led blinking off

uint16_t fas_debug_led_cnt = 0;
uint16_t fas_delta_lsw_A = 0;
uint8_t fas_delta_msb_A = 0;
uint16_t fas_delta_lsw_B = 0;
uint8_t fas_delta_msb_B = 0;
long fas_target_pos_A = 0;
long fas_target_pos_B = 0;
bool fas_dir_cw_A = true;
bool fas_dir_cw_B = true;
long fas_pos_A = 0;
long fas_pos_B = 0;
uint8_t fas_skip_A = 0;
uint8_t fas_skip_B = 0;

FastAccelStepper fas_stepperA = FastAccelStepper(true);
FastAccelStepper fas_stepperB = FastAccelStepper(false);

#define StepperA_Toggle       TCCR1A =  (TCCR1A | _BV(COM1A0)) & ~_BV(COM1A1)
#define StepperA_Zero         TCCR1A =  (TCCR1A | _BV(COM1A1)) & ~_BV(COM1A0)
#define StepperA_Disconnect   TCCR1A =  (TCCR1A & ~(_BV(COM1A1) | _BV(COM1A0)))
#define StepperB_Toggle       TCCR1A =  (TCCR1A | _BV(COM1B0)) & ~_BV(COM1B1)
#define StepperB_Zero         TCCR1A =  (TCCR1A | _BV(COM1B1)) & ~_BV(COM1B0)
#define StepperB_Disconnect   TCCR1A =  (TCCR1A & ~(_BV(COM1B1) | _BV(COM1B0)))

void FastAccelStepperEngine::init() {
   noInterrupts();

   // Set WGM13:0 to all zero => Normal mode
   TCCR1A &= ~(_BV(WGM11) | _BV(WGM10));
   TCCR1B &= ~(_BV(WGM13) | _BV(WGM12));

   // Set prescaler to 1
   TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS11) | _BV(CS10))) | _BV(CS10);

   // enable OVF interrupt
   TIMSK1 |= _BV(TOIE1);

   interrupts();
}
void FastAccelStepperEngine::setDebugLed(uint8_t ledPin) {
   fas_ledPin = ledPin;
}

inline void FastAccelStepper::isr_update_move(unsigned long remaining_steps) {
   bool accelerating = false;
   bool decelerate_to_stop = false;
   bool reduce_speed = false;
   if (remaining_steps <= _deceleration_start) {
      decelerate_to_stop = true;
   }
   else if (_curr_speed < _speed) {
      accelerating = true;
   }
   else if (_curr_speed > _speed) {
      reduce_speed = true;
   }
   long curr_ms = millis();
   long dt_ms = curr_ms - _last_ms;
   _last_ms = curr_ms;
   if (accelerating) {
      _curr_speed += _accel / 1000.0 * dt_ms;
      _curr_speed = min(_curr_speed, _speed);
   }
   if (decelerate_to_stop) {
      _dec_time_ms = max(_dec_time_ms-dt_ms,1.0);
      _curr_speed = 2*remaining_steps * 1000.0/_dec_time_ms;
   }
   if (reduce_speed) {
      _curr_speed -= _accel / 1000.0 * dt_ms;
      _curr_speed = max(_curr_speed, _speed);
   }
   long delta = round(16000000.0/_curr_speed);

   uint16_t x = delta>>14;
   uint16_t delta_lsw;
   if (x > 1) {
      x--;
      delta_lsw = delta & 16383;
      delta_lsw |= 16384;
   }
   else {
      delta_lsw = delta;
   }
   if (_channelA) {
      noInterrupts();
      fas_delta_msb_A = x;
      fas_delta_lsw_A = delta_lsw;
      interrupts();
   }
   else {
      noInterrupts();
      fas_delta_msb_B = x;
      fas_delta_lsw_B = delta_lsw;
      interrupts();
   }
}
ISR(TIMER1_OVF_vect) {
   // disable OVF interrupt to avoid nesting
   digitalWrite(fas_ledPin, HIGH);
   TIMSK1 &= ~_BV(TOIE1);

   long dpA = fas_target_pos_A - fas_pos_A;
   long dpB = fas_target_pos_B - fas_pos_B;

   // enable interrupts for nesting
   interrupts();

   if (fas_ledPin < 255) {
      fas_debug_led_cnt++;
      if (fas_debug_led_cnt == 144) {
        //digitalWrite(fas_ledPin, HIGH);
      }
      if (fas_debug_led_cnt == 288) {
        //digitalWrite(fas_ledPin, LOW);
	fas_debug_led_cnt = 0;
      }
   }
   // Manage stepper A
   if (dpA == 0) {
      uint8_t pin =fas_stepperA.auto_enablePin();
      if (pin != 255) {
        digitalWrite(pin, HIGH);
      }
   }
   else {
      fas_stepperA.isr_update_move(abs(dpA));
      if ((TIMSK1 & _BV(OCIE1A)) == 0) {
	 // motor is not yet running.
	 noInterrupts();
         OCR1A = TCNT1+16000; // delay 1ms for enable to act
	 StepperA_Zero;
         TCCR1C |= _BV(FOC1A); // force compare to ensure cleared output bits
	 StepperA_Toggle;
         fas_skip_A = 0;
         TIFR1 = _BV(OCF1A);    // clear interrupt flag
         TIMSK1 |= _BV(OCIE1A); // enable compare A interrupt
         fas_dir_cw_A = dpA > 0 ? 1 : 0;
         uint8_t pin = fas_stepperA.auto_enablePin();
         if (pin != 255) {
            digitalWrite(pin, LOW);
         }
	 interrupts();
      }
   }

   // Manage stepper B
   if (dpB == 0) {
      uint8_t pin =fas_stepperB.auto_enablePin();
      if (pin != 255) {
        digitalWrite(pin, HIGH);
      }
   }
   else {
      fas_stepperB.isr_update_move(abs(dpB));
      if ((TIMSK1 & _BV(OCIE1B)) == 0) {
	 // motor is not yet running.
	 noInterrupts();
         OCR1B = TCNT1+16000; // delay 1ms for enable to act
	 StepperB_Zero;
         TCCR1C |= _BV(FOC1B); // force compare to ensure cleared output bits
	 StepperB_Toggle;
         fas_skip_B = 0;
         TIFR1 = _BV(OCF1B);    // clear interrupt flag
         TIMSK1 |= _BV(OCIE1B); // enable compare B interrupt
         fas_dir_cw_B = dpB > 0 ? 1 : 0;
         uint8_t pin = fas_stepperB.auto_enablePin();
         if (pin != 255) {
            digitalWrite(pin, LOW);
         }
	 interrupts();
      }
   }

   // enable OVF interrupt
   TIMSK1 |= _BV(TOIE1);
   digitalWrite(fas_ledPin, LOW);
}

ISR(TIMER1_COMPA_vect) {
   if (fas_skip_A) {
      if ((--fas_skip_A) == 0) {
         StepperA_Toggle;
      }
      OCR1A += 16384;
   }
   else {
      TCCR1C = _BV(FOC1A); // clear bit
      // count the pulse
      bool res;
      if (fas_dir_cw_A) {
         res = (++fas_pos_A == fas_target_pos_A);
      } else {
         res = (--fas_pos_A == fas_target_pos_A);
      }
      if (res) {
         StepperA_Disconnect;
         TIMSK1 &= ~_BV(OCIE1A);
      }
      else {
         OCR1A += fas_delta_lsw_A;
         if (fas_skip_A = fas_delta_msb_A) { // assign to skip and test for not zero
            StepperA_Zero;
         }
      }
   }
}

ISR(TIMER1_COMPB_vect) {
   if (fas_skip_B) {
      if ((--fas_skip_B) == 0) {
	 StepperB_Toggle;
      }
      OCR1B += 16384;
   }
   else {
      TCCR1C = _BV(FOC1B); // clear bit
      // count the pulse
      bool res;
      if (fas_dir_cw_B) {
         res = (++fas_pos_B == fas_target_pos_B);
      } else {
         res = (--fas_pos_B == fas_target_pos_B);
      }
      if (res) {
         // set to output zero mode
	 StepperB_Disconnect;
         TIMSK1 &= ~_BV(OCIE1B);
      }
      else {
         OCR1B += fas_delta_lsw_B;
         if (fas_skip_B = fas_delta_msb_B) { // assign to skip and test for not zero
	    StepperB_Zero;
         }
      }
   }
}

FastAccelStepper *FastAccelStepperEngine::stepperA() {
   return &fas_stepperA;
}
FastAccelStepper *FastAccelStepperEngine::stepperB() {
   return &fas_stepperB;
}

FastAccelStepper::FastAccelStepper(bool channelA) {
   _channelA = channelA;
   _auto_enablePin = 255;
   _curr_speed = 0.0;

   uint8_t pin = _channelA ? stepPinStepperA : stepPinStepperB;
   digitalWrite(pin, LOW);
   pinMode(pin, OUTPUT);
}
void FastAccelStepper::setDirectionPin(uint8_t dirPin) {
   _dirPin = dirPin;
   pinMode(dirPin, OUTPUT);
}
void FastAccelStepper::setEnablePin(uint8_t enablePin) {
   _enablePin = enablePin;
   digitalWrite(enablePin, HIGH);
   pinMode(enablePin, OUTPUT);
}
void FastAccelStepper::set_auto_enable(bool auto_enable) {
   if (auto_enable) {
      _auto_enablePin = _enablePin;
   }
   else {
      _auto_enablePin = 255;
   }
}
uint8_t FastAccelStepper::auto_enablePin() {
   return _auto_enablePin;
}
void FastAccelStepper::set_dynamics(float speed, float accel) {
   _speed = speed;
   _accel = accel;
   _min_steps = round(speed*speed/accel);
}
void FastAccelStepper::move(long move) {
   if (_channelA) {
      noInterrupts();
      fas_target_pos_A = fas_pos_A + move;
      interrupts();
   }
   else {
      noInterrupts();
      fas_target_pos_B = fas_pos_B + move;
      interrupts();
   }
   _calculate_move(move);
}
void FastAccelStepper::moveTo(long position) {
   long move;
   if (_channelA) {
      noInterrupts();
      fas_target_pos_A = position;
      move = position - fas_pos_A;
      interrupts();
   }
   else {
      noInterrupts();
      fas_target_pos_B = position;
      move = position - fas_pos_A;
      interrupts();
   }
   _calculate_move(move);
}
void FastAccelStepper::_calculate_move(long move) {
   unsigned long steps = abs(move);
   // The movement consists of three phases.
   // 1. Change current speed to constant speed
   // 2. Constant travel speed
   // 3. Decelerate to stop
   //
   // With v_t being travel speed
   //
   // Steps for 3 (no emergency stop):
   //     t_dec = v_t / a
   //     s_3   = 1/2 * a * t_dec² = v_t² / 2a
   //
   // Steps for 1:
   //     if v <= v_t
   //        t_acc_1 = v_t / a
   //        t_acc_2 = v   / a
   //        s_1 = 1/2 * a * t_acc_1² - 1/2 * a * t_acc_2²
   //            = 1/2 * v_t² / a - 1/2 * v² / a
   //            = (v_t² - v²) / 2a
   //            = s_3 - v^2 / 2a
   //
   //     if v > v_t
   //        s_1 = (v^2 - v_t^2) / 2a
   //            = v^2 / 2a - s_3
   //
   // Steps for 2:
   //     s_2 = steps - s_1 - s_3
   //     if v <= v_t
   //        s_2 = steps - 2 s_3 + v^2 / 2a
   //     if v > v_t
   //        s_2 = steps - v^2 / 2a
   //
   // Case 1: Normal operation
   //     steps >= s_1 + s_3 for a proper v_t
   //     if v <= v_t
   //        steps >= 2 s_3 - v^2 / 2a for a proper v_t
   //     if v > v_t
   //        steps >= v^2 / 2a for v_t = v_max
   //
   // Case 2: Emergency stop
   //     steps < v^2 / 2a
   //     this can be covered by a generic step 3, using constant decelaration a_3:
   //         s_remain = 1/2 * v * t_dec
   //         t_dec = 2 s_remain / v
   //         a_3 = v / t_dec = v^2 / 2 s_remain
   //

   // Steps needed to stop from current speed with defined acceleration
   unsigned long new_deceleration_start;
   unsigned long new_dec_time_ms;
   unsigned long s_stop = round(_curr_speed * _curr_speed / 2.0 / _accel);
   if (s_stop > steps) {
      // start deceleration immediately
      new_deceleration_start = steps;
      new_dec_time_ms = round(2000.0 * steps / _curr_speed);
   }
   else if (_curr_speed <= _speed) {
      // add steps to reach current speed to full ramp
      unsigned long s_full_ramp = steps + s_stop;
      unsigned long ramp_steps = min(s_full_ramp, _min_steps);
      new_deceleration_start = ramp_steps/2;
      new_dec_time_ms = round(sqrt(ramp_steps/_accel)*1000.0);
   }
   else {
      // need decelerate first in phase 1, then normal deceleration
      new_deceleration_start = _min_steps/2;
      new_dec_time_ms = round(_speed/_accel*1000.0);
   }
   noInterrupts();
   _deceleration_start = new_deceleration_start;
   _dec_time_ms = new_dec_time_ms;
   _last_ms = millis();
   interrupts();
}
void FastAccelStepper::disableOutputs() {
   if (_enablePin != 255) {
     digitalWrite(_enablePin, HIGH);
   }
}
void FastAccelStepper::enableOutputs() {
   if (_enablePin != 255) {
     digitalWrite(_enablePin, LOW);
   }
}
long FastAccelStepper::getCurrentPosition() {
   long pos;
   if (_channelA) {
      noInterrupts();
      pos = fas_pos_A;
      interrupts();
   }
   else {
      noInterrupts();
      pos = fas_pos_B;
      interrupts();
   }
   return pos;
}
bool FastAccelStepper::isRunning() {
   bool is_running;
   if (_channelA) {
      noInterrupts();
      is_running = (fas_pos_A != fas_target_pos_A);
      interrupts();
   }
   else {
      noInterrupts();
      is_running = (fas_pos_B != fas_target_pos_B);
      interrupts();
   }
   return is_running;
}