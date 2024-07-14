#include <LiquidCrystal_I2C.h>

constexpr uint8_t LCD_ADDR = 0x27;

constexpr uint8_t START_BUTTON_PORT     = 4;
constexpr uint8_t START_GATE_PORT       = 7;
constexpr uint8_t FINISH_GATE_PORT      = 6;
constexpr uint8_t SEMAPHORE_RED_PORT    = 11;
constexpr uint8_t SEMAPHORE_YELLOW_PORT = 9;
constexpr uint8_t SEMAPHORE_GREEN_PORT  = 10;

constexpr long SEMAPHORE_STOP_TIME      = 1000;
constexpr long SEMAPHORE_READY_MIN_TIME = 1500;
constexpr long SEMAPHORE_READY_MAX_TIME = 2000;

const unsigned int MAX_RACE_COUNT = 5;

constexpr long START_GATE_COOLDOWN = 3000;
constexpr long FINISH_GATE_COOLDOWN = 3000;

constexpr long LINE_MOVE_TIME = 500;
constexpr long LINE_MOVE_STEP = 2;
constexpr long UI_UPDATE_TIME = 300;

char vStringBuffer[21];
const char vLineBuffer[21];
const char vTimeStringBuffer[20];
const char vReactionTimeStringBuffer[10];

constexpr const char* MORE_THAN_SECOND = "(>1s)";

unsigned long vCycleMillis = 0;

//----------------------------------------------------------
//
// Utils
//
//----------------------------------------------------------
const char* time_to_str(unsigned long time) {
  sprintf(
    vTimeStringBuffer,
    "%02d:%02d:%02d",
    (int)((time % 3600000) / 60000),
    (int)((time % 60000) / 1000),
    (int)((time % 1000) / 10)
  );

  return vTimeStringBuffer;
}

const char* _reactionTimeToStr(unsigned long time) {
  if (time > 1000) {
    return MORE_THAN_SECOND;
  } else {
    sprintf(
      vReactionTimeStringBuffer,
      "(%03dms)",
      (int)(time % 1000)
    );

    return vReactionTimeStringBuffer;
  }
}

const char* _toLineBuffer(const char* line) {
  sprintf(
    vLineBuffer,
    "%-20s",
    line
  );

  return vLineBuffer;
}

void semaphoreRed() {
  digitalWrite(SEMAPHORE_RED_PORT, HIGH);
  digitalWrite(SEMAPHORE_YELLOW_PORT, LOW);
  digitalWrite(SEMAPHORE_GREEN_PORT, LOW);
}

void semaphoreYellow() {
  digitalWrite(SEMAPHORE_RED_PORT, LOW);
  digitalWrite(SEMAPHORE_YELLOW_PORT, HIGH);
  digitalWrite(SEMAPHORE_GREEN_PORT, LOW);
}

void semaphoreGreen() {
  digitalWrite(SEMAPHORE_RED_PORT, LOW);
  digitalWrite(SEMAPHORE_YELLOW_PORT, LOW);
  digitalWrite(SEMAPHORE_GREEN_PORT, HIGH);
}

void semaphoreOff() {
  digitalWrite(SEMAPHORE_RED_PORT, LOW);
  digitalWrite(SEMAPHORE_YELLOW_PORT, LOW);
  digitalWrite(SEMAPHORE_GREEN_PORT, LOW);
}

//----------------------------------------------------------
//
// Domain
//
//----------------------------------------------------------
struct SFinish {
  unsigned long vTime;
  bool vFalseStart;
};

class CRace {
public:
  CRace(unsigned int sequence) {
    this->vSequence = sequence;
    this->vStartTime = millis();
  }

  const CRace* falseStart() {
    this->vFalseStart = true;
    this->vOutTime = this->vStartTime;

    return this;
  }

  const CRace* pilotWentFromGates() {
    if (this->vOutTime == 0)
      this->vOutTime = millis();

    return this;
  }

  bool isPilotOut() {
    return this->vOutTime != 0;
  }

  bool isFinished() {
    return this->vStartTime != 0 && this->vFinishTime != 0;
  }

  bool isFalseStarted() {
    return this->vFalseStart;
  }

  unsigned long getTime() {
    if (this->vStartTime == 0) {
      return 0;
    } else if (this->vFinishTime != 0) {
      return this->vFinishTime - this->vStartTime;
    } else {
      return millis() - this->vStartTime;
    }
  }

  unsigned int reactionTime() {
    if (this->vOutTime) {
      return this->vOutTime - this->vStartTime;
    }
  }

  unsigned int getId() {
    return this->vSequence;
  }

  SFinish getFinish() {
    return (SFinish) {
      this->getTime(),
      this->isFalseStarted()
    };
  }

  const CRace* finish() {
    this->vFinishTime = millis();

    return this;
  }

  const char* toString() {
    unsigned long time = this->getTime();
  
    sprintf(vStringBuffer,
      "%d| %02d:%02d:%02d%s %s",
      this->getId(),

      (int)((time % 3600000) / 60000),
      (int)((time % 60000) / 1000),
      (int)((time % 1000) / 10),

      this->isFalseStarted() ? "+P" : "",
  
      this->isPilotOut() && !this->isFalseStarted() ? _reactionTimeToStr(this->reactionTime()) : ""
    );

    return vStringBuffer;
  }

protected:
  unsigned int  vSequence   = 0;
  unsigned long vStartTime  = 0;
  unsigned long vOutTime    = 0;
  unsigned long vFinishTime = 0;
  bool          vFalseStart = false;
};

class CTrack {
public:
  CTrack() {
    for (unsigned int raceIndex = 0; raceIndex < MAX_RACE_COUNT; raceIndex ++) {
      this->vRaceArray[raceIndex] = (CRace*)NULL;
    }
  }

  unsigned int howManyPilotsOnTrack() {
    unsigned int pilotsOnTrack = 0;

    for (unsigned int raceIndex = 0; raceIndex < MAX_RACE_COUNT; raceIndex ++) {
      if ((void*)this->vRaceArray[raceIndex] == NULL) continue;

      if (!this->vRaceArray[raceIndex]->isFinished()) {
        pilotsOnTrack ++;
      }
    }

    return pilotsOnTrack;
  }

  const CRace* lastFinishedRace() {
    for (unsigned int raceIndex = 0; raceIndex < MAX_RACE_COUNT; raceIndex ++) {
      if ((void*)this->vRaceArray[raceIndex] == NULL) continue;

      if (this->vRaceArray[raceIndex]->isFinished()) {
        return this->vRaceArray[raceIndex];
      }
    }

    return NULL;
  }

  const CRace* finishedRaceLastWithOffset(unsigned int index = 0) {
    int skipIfNotNull = index;

    for (unsigned int raceIndex = 0; raceIndex < MAX_RACE_COUNT; raceIndex ++) {
      if ((void*)this->vRaceArray[raceIndex] == NULL) continue;

      if (!this->vRaceArray[raceIndex]->isFinished()) continue;
      if (skipIfNotNull != 0) {
          skipIfNotNull --;
          continue;
      }
      
      return this->vRaceArray[raceIndex];
    }

    return NULL;
  }

  const CRace* finishPretenderRace() {
    CRace* lastPretender = (CRace*)NULL;

    for (unsigned int raceIndex = 0; raceIndex < MAX_RACE_COUNT; raceIndex ++) {
      if ((void*)this->vRaceArray[raceIndex] == NULL) continue;

      if (!this->vRaceArray[raceIndex]->isFinished()) {
        lastPretender = this->vRaceArray[raceIndex];
      }
    }

    return lastPretender;
  }

  void somebodyFinished() {
    CRace* vFinishPretender = this->finishPretenderRace();
    if ((void*)vFinishPretender != NULL) {
      vFinishPretender->finish();
    }
  }

  CRace* lastRace() {
    return this->vRaceArray[0];
  }

  unsigned int nextRaceId() {
    return this->vCurrentId + 1;
  }

  bool isNewRunAvailable() {
    return MAX_RACE_COUNT - 1 > this->howManyPilotsOnTrack();
  }

  const CRace* falseStart() {
    return this->createRace()->falseStart();
  }

  const CRace* startRace() {
    return this->createRace();
  }

protected:
  const CRace* createRace() {
    _clearLastRun();
    this->vCurrentId ++;
    this->vRaceArray[0] = new CRace(this->vCurrentId);

    return this->vRaceArray[0];
  }

  void _clearLastRun() {
    if ((void*)this->vRaceArray[MAX_RACE_COUNT - 1] != NULL) {
      delete this->vRaceArray[MAX_RACE_COUNT - 1];
      this->vRaceArray[MAX_RACE_COUNT - 1] = NULL;
    }

    for (int raceIndex = MAX_RACE_COUNT - 1; raceIndex > 0; raceIndex --) {
      this->vRaceArray[raceIndex] = this->vRaceArray[raceIndex - 1];
    }

    this->vRaceArray[0] = NULL;
  }

private:
  CRace* vRaceArray[MAX_RACE_COUNT];
  unsigned int vCurrentId = 0;
};

class CSemaphore {
public:
  enum ESemaphoreState {
    SEMAPHORE_OFF,
    SEMAPHORE_STOP,
    SEMAPHORE_READY,
    SEMAPHORE_GO,
    SEMAPHORE_ALERT
  };

  CSemaphore() {
    this->_changeState(SEMAPHORE_OFF);
  }

  void turnOff() {
    this->_changeState(SEMAPHORE_OFF);
  }

  void turnOn() {
    this->_changeState(SEMAPHORE_STOP);
  }

  void alert() {
    this->_changeState(SEMAPHORE_ALERT);
  }

  void process() {
    switch (this->eState) {
    case SEMAPHORE_STOP:
      this->_processStop();
      break;
    case SEMAPHORE_READY:
      this->_processReady();
      break;
    case SEMAPHORE_GO:
      this->_processGo();
      break;
    default:
      break;
    };
  }

  bool isOpened() {
    return this->eState == SEMAPHORE_GO;
  }

  ESemaphoreState getState() {
    return this->eState;
  }
protected:
  void _changeState(ESemaphoreState newState) {
    this->eState = newState;
    this->vStateTime = millis();

    switch (this->eState) {
    case SEMAPHORE_STOP:
      semaphoreRed();
      break;
    case SEMAPHORE_READY:
      semaphoreYellow();
      break;
    case SEMAPHORE_GO:
      semaphoreGreen();
      break;
    case SEMAPHORE_OFF:
      semaphoreOff();
    default:
      break;
    };
  }

  unsigned long _stateTime() {
    return millis() - this->vStateTime;
  }

  void _processReady() {
    if (this->_stateTime() > this->vReadyGap) {
      this->_changeState(SEMAPHORE_GO);
    }
  }

  void _processGo() {

  }

  void _processStop() {
    if (this->_stateTime() > SEMAPHORE_STOP_TIME) {
      this->_changeState(SEMAPHORE_READY);
      this->vReadyGap = random(SEMAPHORE_READY_MIN_TIME, SEMAPHORE_READY_MAX_TIME);
    }
  }

private:
  ESemaphoreState eState   = SEMAPHORE_OFF;
  unsigned long vStateTime = 0;
  unsigned long vReadyGap  = 0;
};

class CState {
public:
  enum EState{
    STATE_INIT,
    STATE_READY,
    STATE_COUNTDOWN,
    STATE_GATE_OPEN,
    STATE_GATE_COOLDOWN
  };

  class IHandler {
  public:
    virtual bool isStartButtonOn();
    virtual bool isSomebodyInStartGate();
  };

  CState(IHandler* handler) {
    this->vHandler = handler;
    this->_stateGateCooldown();
  }

  void process() {
    switch (this->vState) {
      case STATE_READY:
        _stateReady();
        break;

      case STATE_COUNTDOWN:
        _stateCountdown();
        break;

      case STATE_GATE_OPEN:
        _stateGateOpen();
        break;

      case STATE_GATE_COOLDOWN:
        _stateGateCooldown();
        break;

      default:
        break;
    }

    this->vSemaphore.process();
  }

  CSemaphore* getSemaphore() {
    return &this->vSemaphore;
  }

  CTrack* getTrack() {
    return &this->vTrack;
  }

  EState getState() {
    return this->vState;
  }

protected:
  void _changeState(EState state) {
    this->vState = state;
    this->vStateTime = millis();
  }

  unsigned long _stateTime() {
    return millis() - this->vStateTime;
  }

  void _stateReady() {
    if (this->vHandler->isStartButtonOn()) {
      this->_changeState(STATE_COUNTDOWN);
      this->vSemaphore.turnOn();
    } else if(this->vHandler->isSomebodyInStartGate()) {
      this->_changeState(STATE_GATE_COOLDOWN);
    }
  }

  void _stateCountdown() {
    if (this->vSemaphore.isOpened()) {
      this->_changeState(STATE_GATE_OPEN);
      this->vTrack.startRace();
      this->process();
    } else if(this->vHandler->isSomebodyInStartGate()) {
      // false start
      this->vSemaphore.alert();
      this->vTrack.falseStart();

      this->_changeState(STATE_GATE_COOLDOWN);
      this->process();
    }
  }

  void _stateGateOpen() {
    if (this->vHandler->isSomebodyInStartGate()) {
      this->vTrack.lastRace()->pilotWentFromGates();

      this->_changeState(STATE_GATE_COOLDOWN);
    }
  }

  void _stateGateCooldown() {
    if (this->vHandler->isSomebodyInStartGate()) {
      this->_changeState(STATE_GATE_COOLDOWN);
    } else if (this->_stateTime() > START_GATE_COOLDOWN && this->vTrack.isNewRunAvailable()) {
      this->_changeState(STATE_READY);
      this->vSemaphore.turnOff();
    }
  }

private:
  unsigned long vStateTime = 0;
  EState        vState = STATE_READY;

  CTrack        vTrack;
  CSemaphore    vSemaphore;

  IHandler*     vHandler;
};

class CFinishGate {
public:
  enum EState {
    FINISH_CLOSED,
    FINISH_READY,
    FINISH_COOLDOWN
  };

  class IHandler {
  public:
    virtual bool isSomebodyInFinishGate();
  };

  CFinishGate(CState* raceState, IHandler* handler) {
    this->vRaceState = raceState;
    this->vHandler = handler;
  }

  void process() {
    switch (this->vState) {
      case FINISH_READY:
        _stateReady();
        break;

      case FINISH_COOLDOWN:
        _stateCooldown();
        break;
      
      default:
        _tryOpen();
        break;
    }
  }

  EState getState() {
    return this->vState;
  }
protected:
  void _changeState(EState state) {
    this->vState = state;
    this->vStateTime = millis();
  }

  unsigned long _stateTime() {
    return millis() - this->vStateTime;
  }

  void _tryOpen() {
    if (this->vHandler->isSomebodyInFinishGate()) {
      this->_changeState(FINISH_COOLDOWN);
    } else if (this->vRaceState->getTrack()->howManyPilotsOnTrack() > 0) {
      this->_changeState(FINISH_READY);
    } else {
      this->_changeState(FINISH_CLOSED);
    }
  }

  void _stateCooldown() {
    if (this->vHandler->isSomebodyInFinishGate()) {
      this->_changeState(FINISH_COOLDOWN);
    } else if (this->_stateTime() > FINISH_GATE_COOLDOWN) {
      this->_tryOpen();
    }
  }

  void _stateReady() {
    if (this->vHandler->isSomebodyInFinishGate()) {
      this->vRaceState->getTrack()->somebodyFinished();

      this->_changeState(FINISH_COOLDOWN);
    }
  }

private:
  EState vState = FINISH_CLOSED;
  unsigned long vStateTime = 0;
  CState* vRaceState;
  IHandler* vHandler;
};

//----------------------------------------------------------
//
// Implementation
//
//----------------------------------------------------------
class CStateHandlerImpl : public CState::IHandler {
public:
  bool isStartButtonOn() {
    return digitalRead(START_BUTTON_PORT) == HIGH;
  }
  bool isSomebodyInStartGate() {
    return digitalRead(START_GATE_PORT) == HIGH;
  }
};

class CFinishGateHandlerImpl : public CFinishGate::IHandler {
public:
  bool isSomebodyInFinishGate() {
    return digitalRead(FINISH_GATE_PORT) == HIGH;
  }
};

CTrack vTrack;
CSemaphore vSemaphore;
CStateHandlerImpl vTelemetryHandler;
CState vState(&vTelemetryHandler);
CFinishGateHandlerImpl vFinishGateHandler;
CFinishGate vFinishGate(&vState, &vFinishGateHandler);

LiquidCrystal_I2C lcd(LCD_ADDR, 20, 4);

//----------------------------------------------------
// Tools and UI
//----------------------------------------------------
class CTimer {
public:
  CTimer(unsigned int updateTime = 1000) {
    this->vTime = millis();
    this->vUpdateTime = updateTime;
  }

  bool isTimeOut() {
    return this->vTime + this->vUpdateTime < millis();
  }
  
  void reset() {
    this->vTime = millis();
  }

protected:
  unsigned long vTime;
  unsigned int  vUpdateTime;
};

void setup() {
  lcd.init();
  lcd.backlight();

  lcd.setCursor(2, 1);
  lcd.print("MotoGymkhana73");

  Serial.begin(9600);

  pinMode(START_BUTTON_PORT, INPUT);
  pinMode(START_GATE_PORT, INPUT);
  pinMode(FINISH_GATE_PORT, INPUT);

  pinMode(SEMAPHORE_RED_PORT, OUTPUT);
  pinMode(SEMAPHORE_YELLOW_PORT, OUTPUT);
  pinMode(SEMAPHORE_GREEN_PORT, OUTPUT);
  semaphoreOff();

  lcd.clear();
}

//----------------------------------------------------------
//
// UI (Temporary)
//
//----------------------------------------------------------
CTimer    vUITimer(UI_UPDATE_TIME);

constexpr const char* TEXT_READY = "Ready";
constexpr const char* TEXT_START_GATE_COOLDOWN = "Wait start gate";
constexpr const char* TEXT_FINISH_GATE_COOLDOWN = "Wait finish gate";
constexpr const char* TEXT_GATE_OPEN = "Race started";
constexpr const char* TEXT_EMPTY = "";
const char* vTextState = "";

char vSpaceLine[21] = "                    ";
char vLcdLines[4][21] = {"", "", "", ""};
int vTextLineWritten = 0;

void printLine(unsigned int lineNum, const char * text) {
  if (strcmp(vLcdLines[lineNum], text) == 0) return;

  lcd.setCursor(0, lineNum);
  vTextLineWritten = lcd.print(text);

  if (strlen(vLcdLines[lineNum]) > strlen(text) && vTextLineWritten < 20) {
    vSpaceLine[20 - vTextLineWritten] = 0;
    lcd.print(vSpaceLine);
    vSpaceLine[20 - vTextLineWritten] = " ";
  }

  strcpy(vLcdLines[lineNum], text);
}

void showRaceInfo() {
  switch(vState.getState()) {
    case CState::STATE_READY:
      if (vFinishGate.getState() == CFinishGate::FINISH_COOLDOWN && vTrack.howManyPilotsOnTrack() == 0) {
        vTextState = TEXT_FINISH_GATE_COOLDOWN;
      } else {
        vTextState = TEXT_READY;
      }
      break;
    
    case CState::STATE_GATE_COOLDOWN:
      vTextState = TEXT_START_GATE_COOLDOWN;
      break;

    case CState::STATE_GATE_OPEN:
      vTextState = TEXT_GATE_OPEN;
      break;

    case CState::STATE_COUNTDOWN:
      vTextState = TEXT_GATE_OPEN;
      break;

    default:
      vTextState = TEXT_EMPTY;
  }

  sprintf(vStringBuffer,
    "%d| %s",
    vState.getTrack()->nextRaceId(),
    vTextState
  );

  printLine(0, vStringBuffer);
}

void showRaceTimes() {
  CRace* race = vState.getTrack()->lastRace();
  if (race && !race->isFinished()) {
    lcd.setCursor(0, 1);
    printLine(1, race->toString());
  } else {
    printLine(1, "No one on track");
  }
  
  race = vState.getTrack()->lastFinishedRace();
  if (race) {
    lcd.setCursor(0, 2);
    printLine(2, race->toString());
  }

  race = vState.getTrack()->finishedRaceLastWithOffset(1);
  if (race) {
    lcd.setCursor(0, 3);
    printLine(3, race->toString());
  }
}

//----------------------------------------------------------
//
// Loop
//
//----------------------------------------------------------
void loop() {
  vState.process();
  vFinishGate.process();

  if (vUITimer.isTimeOut()) {
    vUITimer.reset();

    showRaceInfo();
    showRaceTimes();
  }
}
