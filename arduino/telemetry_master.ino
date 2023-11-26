#include <LiquidCrystal_I2C.h>

constexpr uint8_t LCD_ADDR = 0x27;

constexpr uint8_t START_BUTTON_PORT     = 13;
constexpr uint8_t START_GATE_PORT       = 12;
constexpr uint8_t FINISH_GATE_PORT      = 11;
constexpr uint8_t SEMAPHORE_RED_PORT    = 4;
constexpr uint8_t SEMAPHORE_YELLOW_PORT = 3;
constexpr uint8_t SEMAPHORE_GREEN_PORT  = 2;

constexpr long SEMAPHORE_STOP_TIME      = 1000;
constexpr long SEMAPHORE_READY_MIN_TIME = 1500;
constexpr long SEMAPHORE_READY_MAX_TIME = 4000;

const unsigned int MAX_RACE_COUNT = 5;

constexpr long START_GATE_COOLDOWN = 5000;
constexpr long FINISH_GATE_COOLDOWN = 5000;

const char vStringBuffer[20];

//----------------------------------------------------------
//
// Utils
//
//----------------------------------------------------------
char* time_to_str(unsigned long time) {
  sprintf(
    vStringBuffer,
    "%02d:%02d:%02d",
    (int)((time % 3600000) / 60000),
    (int)((time % 60000) / 1000),
    (int)((time % 1000) / 10)
  );

  return vStringBuffer;
}

void semaphoreRed() {
  digitalWrite(SEMAPHORE_RED_PORT, LOW);
  digitalWrite(SEMAPHORE_YELLOW_PORT, HIGH);
  digitalWrite(SEMAPHORE_GREEN_PORT, HIGH);
}

void semaphoreYellow() {
  digitalWrite(SEMAPHORE_RED_PORT, HIGH);
  digitalWrite(SEMAPHORE_YELLOW_PORT, LOW);
  digitalWrite(SEMAPHORE_GREEN_PORT, HIGH);
}

void semaphoreGreen() {
  digitalWrite(SEMAPHORE_RED_PORT, HIGH);
  digitalWrite(SEMAPHORE_YELLOW_PORT, HIGH);
  digitalWrite(SEMAPHORE_GREEN_PORT, LOW);
}

void semaphoreOff() {
  digitalWrite(SEMAPHORE_RED_PORT, HIGH);
  digitalWrite(SEMAPHORE_YELLOW_PORT, HIGH);
  digitalWrite(SEMAPHORE_GREEN_PORT, HIGH);
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
  CRace(unsigned int sequence, bool falseStart = false) {
    this->vSequence = sequence;
    this->vStartTime = millis();
    this->vFalseStart = falseStart;
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

  unsigned int getId() {
    return this->vSequence;
  }

  SFinish getFinish() {
    return (SFinish) {
      this->getTime(),
      this->isFalseStarted()
    };
  }

  void finish() {
    this->vFinishTime = millis();
  }

protected:
  unsigned int  vSequence   = 0;
  unsigned long vStartTime  = 0;
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

  CRace* lastFinishedRace() {
    for (unsigned int raceIndex = 0; raceIndex < MAX_RACE_COUNT; raceIndex ++) {
      if ((void*)this->vRaceArray[raceIndex] == NULL) continue;

      if (this->vRaceArray[raceIndex]->isFinished()) {
        return this->vRaceArray[raceIndex];
      }
    }

    return NULL;
  }

  CRace* finishPretenderRace() {
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

  void startRace(bool falseStart = false) {
    _clearLastRun();
    this->vCurrentId ++;
    this->vRaceArray[0] = new CRace(this->vCurrentId, falseStart);
  }

protected:
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
    this->_changeState(STATE_READY);
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
      this->vTrack.startRace(true);

      this->_changeState(STATE_GATE_COOLDOWN);
      this->process();
    }
  }

  void _stateGateOpen() {
    if (this->vHandler->isSomebodyInStartGate()) {
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
    if (this->vRaceState->getTrack()->howManyPilotsOnTrack() > 0) {
      this->_changeState(FINISH_READY);
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
void showMainState() {
  lcd.setCursor(0, 2);

  switch (vState.getState()) {
    case CState::EState::STATE_READY:
      lcd.print("Ready    ");
      break;
    case CState::EState::STATE_COUNTDOWN:
      lcd.print("Countdown");
      break;
    case CState::EState::STATE_GATE_OPEN:
      lcd.print("Go       ");
      break;
    case CState::EState::STATE_GATE_COOLDOWN:
      lcd.print("Prepare  ");
      break;
    default:
      lcd.print("Error    ");
      break;
  }

  sprintf(
    vStringBuffer,
    " | Next: %02d",
    vState.getTrack()->nextRaceId()
  );
  lcd.print(vStringBuffer);
}

void showSemaphoreState() {
  lcd.setCursor(0, 3);

  const char* vSemaphoreStateString;
  switch (vState.getSemaphore()->getState()) {
    case CSemaphore::ESemaphoreState::SEMAPHORE_STOP:
      vSemaphoreStateString = "Red   ";
      break;
    case CSemaphore::ESemaphoreState::SEMAPHORE_READY:
      vSemaphoreStateString = "Yellow";
      break;
    case CSemaphore::ESemaphoreState::SEMAPHORE_GO:
      vSemaphoreStateString = "Green ";
      break;
    default:
      vSemaphoreStateString = "Closed";
      break;
  }

  const char *vFinishStateString;
  switch (vFinishGate.getState()) {
    case CFinishGate::EState::FINISH_READY:
      vFinishStateString = "Open  ";
      break;
    case CFinishGate::EState::FINISH_COOLDOWN:
      vFinishStateString = "Closed";
      break;
    default:
      vFinishStateString = "Closed";
      break;
  }

  char buff[20];
  sprintf(buff, "Gates: %s/%s", vSemaphoreStateString, vFinishStateString);
  lcd.print(buff);
}

void showCurrentRun() {
  lcd.setCursor(0, 1);
  char buff[20];
  CRace* firstFinishPretender = vState.getTrack()->finishPretenderRace();

  if ((void*)firstFinishPretender == NULL) {
    lcd.print("                    ");
    return;
  }

  unsigned long raceTime = firstFinishPretender->getTime();

  sprintf(buff, "%d|", firstFinishPretender->getId());
  lcd.print(buff);
  lcd.print(time_to_str(raceTime));

  if (firstFinishPretender->isFalseStarted()) {
    lcd.print("+P");
  } else {
    lcd.print("  ");
  }
}

void showLastRun() {
  lcd.setCursor(0, 0);

  CRace* lastFinishedRace = vState.getTrack()->lastFinishedRace();

  if ((void*)lastFinishedRace == NULL) {
    lcd.print("                    ");
    return;
  }

  char buff[20];
  unsigned long raceTime = lastFinishedRace->getTime();

  sprintf(buff, "%d|", lastFinishedRace->getId());
  lcd.print(buff);
  lcd.print(time_to_str(raceTime));

  if (lastFinishedRace->isFalseStarted()) {
    lcd.print("+P");
  } else {
    lcd.print("  ");
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

  showLastRun();
  showCurrentRun();
  showMainState();
  showSemaphoreState();
}