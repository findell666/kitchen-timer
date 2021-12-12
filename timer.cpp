/////**** CLEMEMENt****///
//faire .h
//sleep mode if unused for x time, réveil avec un bouton, low power
//amélioration temps de reponse boutons
//monitor voltage + indicator
//faire non blocking delay

#include <Arduino.h>
#include <GxEPD.h>
#include <GxGDEH0213B73/GxGDEH0213B73.h>  // 2.13" b/w newer panel
#include <Fonts/FreeSansBold18pt7b.h>
///#include <Fonts/FreeMono12pt7b.h>
///#include <Fonts/FreeSansBold9pt7b.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

#define DEBOUNCE_TIME 400
#define DEBOUNCE_TIME_LED 50

#define BUZZZER_PIN 22
#define LED_PIN 27
#define BUTTON_START_STOP 26
#define BUTTON_PLUS_ONE 32
#define BUTTON_PLUS_FIVE 25

#define TIMER_ALARM 0
#define TIMER_COUNT 1
#define TIMER_LED 2

#define STATE_NORMAL 0
#define STATE_SHORT 1
#define STATE_LONG 2

const bool isDebug = false;
const int MAX_RINGINGS = 7;
const int ONE_MINUT = 60;

uint32_t debounceTimer0 = 0;
uint32_t debounceTimer1 = 0;
uint32_t debounceTimer2 = 0;
uint32_t debounceLed = 0;

volatile int nbSeconds = 0;
volatile int count = 0;
volatile bool isStarted = false;
volatile bool isRinging = false;

volatile bool nbSecondsChanged = 0;

int currentRingingsNb = 0;

volatile int buttonStartStopState = 0;
volatile int buttonPlusOneState = 0;
volatile int buttonPlusFiveState = 0;

volatile int prevHours = -1;
volatile int prevMinutes = -1;
volatile int prevSeconds = -1;


unsigned long delayStart = 0; // the time the delay started
bool delayRunning = false; // true if still waiting for delay to finish

/**TIMER**/
hw_timer_t * timerCount = NULL;

portMUX_TYPE timerMuxAlarm = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE timerMuxCount = portMUX_INITIALIZER_UNLOCKED;

const float note[12] = {65.41, 69.30, 73.42, 77.78, 82.41, 87.31, 92.50, 98.00, 103.83, 110.00, 116.54, 123.47};
const int nombreDeNotes = 32;
const int tempo = 150; // plus c'est petit, plus c'est rapide
const int melodie[][3] = { {4, 2, 2}, {5, 2, 1}, {7, 2, 3}, {0, 3, 6}, 
{2, 2, 2}, {4, 2, 1},{5, 2, 8}, 
{7, 2, 2},  {9, 2, 1},  {11, 2, 3},  {5, 3, 6},
{9, 2, 2}, {11, 2, 1}, {0, 3, 3}, {2, 3, 3}, {4, 3, 3},
{4, 2, 2}, {5, 2, 1}, {7, 2, 3}, {0, 3, 6},
{2, 3, 2}, {4, 3, 1},{5, 3, 8}, 
{7, 2, 2}, {7, 2, 1}, {4, 3, 3}, {2, 3, 2},
{7, 2, 1}, {5, 3, 3}, {4, 3, 2}, {2, 3, 1},{0, 3, 8}
};

/*** SCREEN ***/
const int heightCursorOffset = 20;
GxIO_Class io(SPI, SS, 17, 16);
GxEPD_Class display(io, 16, 4);

void printDebugString(char* str){
	if(isDebug){
		Serial.println(str);
	}	
}
void printDebugInt(int integer){
	if(isDebug){
		Serial.println(integer, DEC);
	}	
}

void startTimerCount(int timeInSecs){	
	timerCount = timerBegin(TIMER_COUNT, 80, true);                
	timerAttachInterrupt(timerCount, &onTimeCount, true);		
	timerAlarmWrite(timerCount, 1000000*timeInSecs, true);
	timerAlarmEnable(timerCount);
}

//à appeler dans un mux timerAlarm
void resetAll(){
	portENTER_CRITICAL_ISR(&timerMuxCount);
	count = 0;
	portEXIT_CRITICAL_ISR(&timerMuxCount);
	timerAlarmDisable(timerCount);
	nbSeconds = 0;
	isRinging = false;
	isStarted = false;
	prevHours = -1;
	prevMinutes = -1;
	prevSeconds = -1;
	nbSecondsChanged = 0;
	currentRingingsNb = 0;
}

//à utiliser dans mux alarm
void startCountDown(){
	startTimerCount(1);
	isStarted = true;	
}

/** avec snooze**/
void buttonPlusOnePressed() {	
	if ( millis() - DEBOUNCE_TIME  >= debounceTimer1 ) {	
		digitalWrite(LED_PIN, HIGH);
		debounceTimer1 = millis();		
		buttonPlusOneState = STATE_SHORT;

		//restart timer when ringing
		if(isRinging){
			portENTER_CRITICAL_ISR(&timerMuxAlarm);
			resetAll();
			addMoreTime(ONE_MINUT);
			startCountDown();			
			portEXIT_CRITICAL_ISR(&timerMuxAlarm);
		}
		//add some seconds otherwise
		else{
			portENTER_CRITICAL_ISR(&timerMuxAlarm);
			addMoreTime(ONE_MINUT);
			portEXIT_CRITICAL_ISR(&timerMuxAlarm);
		}	
	}
}

void buttonPlusFivePressed() {	
	if ( millis() - DEBOUNCE_TIME  >= debounceTimer1 ) {	
		digitalWrite(LED_PIN, HIGH);
		debounceTimer1 = millis();		
		buttonPlusFiveState = STATE_SHORT;

		//restart timer when ringing
		if(isRinging){
			portENTER_CRITICAL_ISR(&timerMuxAlarm);
			resetAll();
			addMoreTime(5*ONE_MINUT);
			startCountDown();			
			portEXIT_CRITICAL_ISR(&timerMuxAlarm);
		}
		//add some seconds otherwise
		else{
			portENTER_CRITICAL_ISR(&timerMuxAlarm);
			addMoreTime(5*ONE_MINUT);
			portEXIT_CRITICAL_ISR(&timerMuxAlarm);
		}	
	}
}

void pauseCountDown(){
	printDebugString("Pausing ...");
	timerAlarmDisable(timerCount);
	isStarted = false;
	isRinging = false;
	portENTER_CRITICAL_ISR(&timerMuxCount);
	count = 0;
	portEXIT_CRITICAL_ISR(&timerMuxCount);		
}

void execShortPressButtonStartStop(){
	
	if(isStarted){			
		portENTER_CRITICAL_ISR(&timerMuxAlarm);
		pauseCountDown();
		portEXIT_CRITICAL_ISR(&timerMuxAlarm);
	}
	else{
		//start timer avec la valeur			
		if(nbSeconds > 0){			
			portENTER_CRITICAL_ISR(&timerMuxAlarm);
			startCountDown();
			portEXIT_CRITICAL_ISR(&timerMuxAlarm);
		}
		else{
			printDebugString("Nothing to do");
		}
	}
}

IRAM_ATTR void onTimeCount() {
   portENTER_CRITICAL_ISR(&timerMuxCount);
   count++;
   portEXIT_CRITICAL_ISR(&timerMuxCount);
}

IRAM_ATTR void  buttonStartStopPressed() {	
	if ( millis() - DEBOUNCE_TIME  >= debounceTimer0 ) {
		debounceTimer0 = millis();
		digitalWrite(LED_PIN, HIGH);		
		buttonStartStopState = STATE_SHORT;
		
		if(isRinging){
			printDebugString("Stop the ringing and reset counter");
			portENTER_CRITICAL_ISR(&timerMuxAlarm);
			resetAll();
			portEXIT_CRITICAL_ISR(&timerMuxAlarm);
		}		
	}
}

IRAM_ATTR void addMoreTime(int addedTime){
	if(nbSecondsChanged == 0){
		if(!isRinging){
			nbSeconds += addedTime;
		}
		if(!isStarted){	
			nbSecondsChanged += 1;
		}
	}
}

IRAM_ATTR void displayTime(int total){	
	int WIDTH = display.width();
	int HEIGHT = display.height();		
	
	int width3rd = (int) (WIDTH / 3);
	int cursorHeightPos = (HEIGHT - HEIGHT /2) + heightCursorOffset;
	int baseHeight = 32;
	int upHeight = 55;
	
	int minutes = total / 60;
	int seconds = total % 60;
	int hours = minutes / 60;
	minutes = minutes % 60;

	int nbToRefresh = 0;
	//refreshSeconds
	if(prevSeconds != seconds){
		display.setCursor(width3rd*2, cursorHeightPos);
		display.fillRect(width3rd*2, baseHeight, WIDTH, upHeight, GxEPD_WHITE);		
		char buffer[3];
		sprintf(buffer, "%02d", seconds);		
		display.println(buffer);
		prevSeconds = seconds;
		nbToRefresh = 1;
	}

	//refreshMinutes
	if(prevMinutes != minutes){
		display.setCursor(width3rd, cursorHeightPos);
		display.fillRect(width3rd, baseHeight, width3rd, upHeight, GxEPD_WHITE);
		char buffer[3];
		sprintf(buffer, "%02d", minutes);		
		display.println(buffer);
		prevMinutes = minutes;
		nbToRefresh = 2;
	}
	//refreshHours
	if(prevHours != hours){
		display.setCursor(0, cursorHeightPos);
		display.fillRect(0, baseHeight, width3rd, upHeight, GxEPD_WHITE);
		char buffer[3];
		sprintf(buffer, "%02d", hours);		
		display.println(buffer);
		prevHours = hours;
		nbToRefresh = 3;
	}
	if(nbToRefresh == 1){
		display.updateWindow(width3rd*2, baseHeight, WIDTH, upHeight);
	}
	else if(nbToRefresh == 2){
		display.updateWindow(width3rd, baseHeight, WIDTH, upHeight);
	}
	else if(nbToRefresh == 3){
		display.updateWindow(0, baseHeight, WIDTH, upHeight);
	}
}

void playMelody(){
	int frequence;
	for ( int i = 0; i < nombreDeNotes ; i++ ) {
		if(isRinging){					
			printDebugString("CA SONNE");
			digitalWrite(LED_PIN, HIGH);
			frequence = round(note[melodie[i][0]] * 2.0 * (melodie[i][1] - 1));
			ledcSetup(0, frequence, 12);   
			ledcWrite(0, 2048);  // rapport cyclique 50%
			delay(tempo * melodie[i][2] - 50);
			ledcWrite(0, 0); // rapport cyclique 0% (silence, pour séparer les notes adjacentes)
			delay(50);
			digitalWrite(LED_PIN, LOW);
		}else{
			digitalWrite(LED_PIN, LOW);
			ledcWrite(0, 0);
		}
	}	
}

void setup() {
	if(isDebug){
		Serial.begin(115200);
	}	
	/*** SCREEN ****/	
	display.init();
	display.setTextColor(GxEPD_BLACK);
	display.setRotation(1);
	display.fillScreen(GxEPD_WHITE);
	display.setTextSize(2);		
	display.setFont(&FreeSansBold18pt7b);
	display.eraseDisplay();
	displayTime(nbSeconds);
	
	pinMode(LED_PIN, OUTPUT);	
	digitalWrite(LED_PIN, HIGH);
	ledcAttachPin(BUZZZER_PIN, 0);	
	digitalWrite(LED_PIN, LOW);
   
	pinMode(BUTTON_START_STOP, INPUT_PULLDOWN);	
	attachInterrupt(BUTTON_START_STOP, buttonStartStopPressed, RISING);

	pinMode(BUTTON_PLUS_ONE, INPUT_PULLDOWN);
	attachInterrupt(BUTTON_PLUS_ONE, buttonPlusOnePressed, RISING);

	pinMode(BUTTON_PLUS_FIVE, INPUT_PULLDOWN);
	attachInterrupt(BUTTON_PLUS_FIVE, buttonPlusFivePressed, RISING);
	
	digitalWrite(LED_PIN, LOW);
}

void loop() {

	if(nbSecondsChanged > 0){
		displayTime(nbSeconds);	
		nbSecondsChanged -= 1;
	}

	if(STATE_SHORT == buttonStartStopState){		
		buttonStartStopState = STATE_NORMAL;
		execShortPressButtonStartStop();
		debounceLed = millis();
	}
	
	if(STATE_SHORT == buttonPlusOneState){
		buttonPlusOneState = STATE_NORMAL;
		debounceLed = millis();
	}
	
	if(!isRinging){
		if(millis() - DEBOUNCE_TIME_LED >= debounceLed) {
			debounceLed = millis();
			digitalWrite(LED_PIN, LOW);
		}
	}
	
	//afficahge du decrease du timer alarm quand celui-ci est lancé
	if(isStarted && count > 0){		
		//ici on va afficher le décompte	
		portENTER_CRITICAL_ISR(&timerMuxCount);
		count--;
		portEXIT_CRITICAL_ISR(&timerMuxCount);
		portENTER_CRITICAL_ISR(&timerMuxAlarm);
		printDebugInt(nbSeconds);
		nbSeconds--;
		portEXIT_CRITICAL_ISR(&timerMuxAlarm);
		displayTime(nbSeconds);
		
		//déclencher la sonnerie
		if(nbSeconds == 0){
			isRinging = true;	
		}
	}
	
	if(isRinging){

		isStarted = false;
		
		if(currentRingingsNb >= MAX_RINGINGS){
			currentRingingsNb = 0;
			portENTER_CRITICAL_ISR(&timerMuxAlarm);
			resetAll();
			portEXIT_CRITICAL_ISR(&timerMuxAlarm);
			delayRunning = false;
		}
		else{
			currentRingingsNb++;
			playMelody();
			delay(2000);
		}
	}
		
}
