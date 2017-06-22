/* Standard includes */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

/* HiFive1/FE310 includes */
#include "platform.h"
#include "encoding.h"

#define RTC_FREQUENCY 32768
#define MSG_LEN       100
#define PWM_SCALE     0x04
#define CROCHET       0.50 // This is the length of a single beat

const char * hello_msg = "From Russia with fun!! ...\n";
const char * loop_msg = "End-of-score, replaying ...\n";
char message[MSG_LEN];
bool PWM_enabled = false;

typedef enum {
  NOTE_TIME_QUAVER = 0,
  NOTE_TIME_CROCHET,
  NOTE_TIME_DOTTEDHALF
} note_time_t;

uint64_t note_time_table[] = {
  ((CROCHET/2)  *RTC_FREQUENCY), // NOTE_TIME_QUAVER, half a beat
  (CROCHET      *RTC_FREQUENCY), // NOTE_TIME_CROCHET, full beat
  ((CROCHET*1.5)*RTC_FREQUENCY)  // NOTE_TIME_HALFDOTTED, one-and-a-half beats
};

typedef enum {
  NOTE_FREQ_A = 0, // A_4, f=440Hz,    t=2.27ms
  NOTE_FREQ_B,     // B_4, f=493.88Hz, t=2.02ms
  NOTE_FREQ_C,     // C_5, f=523.25Hz, t=1.91ms
  NOTE_FREQ_D,     // D_5, f=587.33Hz, t=1.70ms
  NOTE_FREQ_E      // E_5, f=659.25Hz, t=1.52ms
} note_freq_t;

/* Table assumes a core frequency of ~262MHz and pwmscale of 4.
 * Integer value is a result of:
 *
 * (Clock source / divisor) / FREQUENCY
 *
 * Eg., A_4:
 *
 * (262MHz / 2^4) / 440 = 37216
 */
uint64_t note_freq_table[] = {
  37216, // NOTE_FREQ_A
  33156, // NOTE_FREQ_B
  31295, // NOTE_FREQ_C
  27880, // NOTE_FREQ_D
  24839  // NOTE_FREQ_E
};

/* Define a type to represent a musical note by
 * specifying its length and frequency
 */
typedef struct note_t {
  note_time_t time;
  note_freq_t freq;
} musical_note_t;

/* Define a musical sample by specifying each note. Assumes only a
 * single channel, but there's not reason why other PWM devices can't
 * be used.
 * */
musical_note_t korobeiniki[] = {
  {NOTE_TIME_CROCHET,    NOTE_FREQ_E},
  {NOTE_TIME_QUAVER,     NOTE_FREQ_B},
  {NOTE_TIME_QUAVER,     NOTE_FREQ_C},
  {NOTE_TIME_CROCHET,    NOTE_FREQ_D},
  {NOTE_TIME_QUAVER,     NOTE_FREQ_C},
  {NOTE_TIME_QUAVER,     NOTE_FREQ_B},
  {NOTE_TIME_CROCHET,    NOTE_FREQ_A},
  {NOTE_TIME_QUAVER,     NOTE_FREQ_A},
  {NOTE_TIME_QUAVER,     NOTE_FREQ_C},
  {NOTE_TIME_CROCHET,    NOTE_FREQ_E},
  {NOTE_TIME_QUAVER,     NOTE_FREQ_D},
  {NOTE_TIME_QUAVER,     NOTE_FREQ_C},
  {NOTE_TIME_DOTTEDHALF, NOTE_FREQ_B},
  {NOTE_TIME_QUAVER,     NOTE_FREQ_C},
  {NOTE_TIME_CROCHET,    NOTE_FREQ_D},
  {NOTE_TIME_CROCHET,    NOTE_FREQ_E},
  {NOTE_TIME_CROCHET,    NOTE_FREQ_C},
  {NOTE_TIME_CROCHET,    NOTE_FREQ_A}
};
uint32_t score_index = 0; // Keeps track of progression through the musical sample

const char * get_note_time_string(note_time_t note)
{
  switch (note) {
    case NOTE_TIME_QUAVER:     return "Quaver";
    case NOTE_TIME_CROCHET:    return "Crochet";
    case NOTE_TIME_DOTTEDHALF: return "Half-dotted";
    default: return "Unknown";
  }
}

const char * get_note_freq_string(note_freq_t note)
{
  switch (note) {
    case NOTE_FREQ_A: return "A_4";
    case NOTE_FREQ_B: return "B_4";
    case NOTE_FREQ_C: return "C_5";
    case NOTE_FREQ_D: return "D_5";
    case NOTE_FREQ_E: return "E_5";
    default: return "Unknown";
  }
}

const char * get_note_freqHZ_string(note_freq_t note)
{
  switch (note) {
    case NOTE_FREQ_A: return "440Hz";
    case NOTE_FREQ_B: return "493.88Hz";
    case NOTE_FREQ_C: return "523.25Hz";
    case NOTE_FREQ_D: return "587.33Hz";
    case NOTE_FREQ_E: return "659.25Hz";
    default: return "Unknown";
  }
}

void handle_m_time_interrupt()
{
  /* Disable the machine and timer interrupts until setup is completed */
  clear_csr(mie, MIP_MEIP);

  /* Set the machine timer to go off in X seconds */
  volatile uint64_t * mtime    = (uint64_t*) (CLINT_BASE_ADDR + CLINT_MTIME);
  volatile uint64_t * mtimecmp = (uint64_t*) (CLINT_BASE_ADDR + CLINT_MTIMECMP);
  uint64_t now = *mtime;
  uint64_t then = now + note_time_table[korobeiniki[score_index].time];
  *mtimecmp = then;

  memset(message, 0, MSG_LEN);
  snprintf(message, MSG_LEN, "Playing note: [ %s %s, %s ]\n",
      get_note_freq_string(korobeiniki[score_index].freq),
      get_note_freqHZ_string(korobeiniki[score_index].freq),
      get_note_time_string(korobeiniki[score_index].time)
  );
  write(STDOUT_FILENO, message, strlen(message));

  /* Apply PWM frequency and enable PWM output pin if this the first note */
  PWM1_REG(PWM_CMP0) = note_freq_table[korobeiniki[score_index].freq];
  PWM1_REG(PWM_CMP2) = note_freq_table[korobeiniki[score_index].freq] / 2; // 50% duty cycle
  PWM1_REG(PWM_CMP3) = note_freq_table[korobeiniki[score_index].freq] / 2; // 50% duty cycle
  if (!PWM_enabled) {
    PWM_enabled = true;
    GPIO_REG(GPIO_IOF_EN) |= (1 << BLUE_LED_OFFSET);
    GPIO_REG(GPIO_IOF_EN) |= (1 << RED_LED_OFFSET);
  }

  /* Increment or reset score index */
  score_index++;
  if (score_index >= 17) { // 17 is the number of notes in this score
    score_index = 0;
    write(STDOUT_FILENO, loop_msg, strlen(loop_msg));
  }
  /* Re-enable timers */
  set_csr(mie, MIP_MTIP);
}

void handle_m_ext_interrupt()
{
}

void start_demo()
{
  /* Disable the machine and timer interrupts until setup is completed */
  clear_csr(mie, MIP_MEIP);
  clear_csr(mie, MIP_MTIP);

  /* Set the machine timer to go off in X seconds */
  volatile uint64_t * mtime    = (uint64_t*) (CLINT_BASE_ADDR + CLINT_MTIME);
  volatile uint64_t * mtimecmp = (uint64_t*) (CLINT_BASE_ADDR + CLINT_MTIMECMP);
  uint64_t now = *mtime;
  uint64_t then = now + 1.5*RTC_FREQUENCY;
  *mtimecmp = then;

  /* Configure PWM  */
  PWM1_REG(PWM_CFG) = 0; // Clear the configuration register
  /* This is the real meat of things.
   */
  PWM1_REG(PWM_CFG) =
    (PWM_CFG_ENALWAYS) |
    (PWM_CFG_DEGLITCH) |
    (PWM_CFG_CMP2CENTER) |
    (PWM_CFG_CMP3CENTER) |
    (PWM_CFG_ZEROCMP) |
    (PWM_SCALE);
  PWM1_REG(PWM_COUNT) = 0;

  /* Enable PWM output pins. No particular reason why I enabled both
   * the blue and red LEDs except that I think the purple looks kind
   * of neat.
   */
  GPIO_REG(GPIO_IOF_SEL) |= (1 << BLUE_LED_OFFSET);
  GPIO_REG(GPIO_IOF_SEL) |= (1 << RED_LED_OFFSET);

  /* Re-enable timers */
  set_csr(mie, MIP_MTIP);
  set_csr(mstatus, MSTATUS_MIE);
}

int main()
{
  write(STDOUT_FILENO, hello_msg, strlen(hello_msg));

  start_demo();

  while (1) {};

  return 0;
}
