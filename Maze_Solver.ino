// IR sensors pin assignment
#define S0 A0
#define S1 A1
#define S2 A2
#define S3 A3
#define S4 A4

// Motor driver pin assignment
#define IN1 8
#define IN2 9
#define IN3 10
#define IN4 11
#define ENA 5
#define ENB 6

// Speeds assignemnt
int baseSpeed = 60;
int turnSpeed = 90;

// PD controller variables for smooth movement
float Kp = 50;
float Kd = 60;

// Path tracking vector creation
char path[200] = "";
int pathLength = 0;
bool mazeSolved = false;
bool runOptimizedMode = false;
int pathIndex = 0;            

// Creating an inIntersection variable to track whether the robot got out of the
// intersection or not to avoid any messy direction recordings, i.e to avoid repeating same direction while still turning on the intersection
bool inIntersection = false;
bool explorationStarted = false;

// Different sensor states
int LeftFar, LeftNear, Center, RightNear, RightFar;

// Global orientation methodology
enum Direction { NORTH = 0, EAST = 1, SOUTH = 2, WEST = 3 };

Direction startDirection = NORTH; 
Direction currentDirection = startDirection;

char getGlobalHeading() {
  switch (currentDirection) {
    case NORTH: return 'N';
    case EAST:  return 'E';
    case SOUTH: return 'S'; 
    case WEST:  return 'W';
    default:    return '?';
  }
}

void setup() {
  Serial3.begin(9600);

  pinMode(S0, INPUT);
  pinMode(S1, INPUT);
  pinMode(S2, INPUT);
  pinMode(S3, INPUT);
  pinMode(S4, INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  Serial3.print("Robot Started (Facing ");
  Serial3.print(getGlobalHeading());
  Serial3.println(")");
  Serial3.println("Type 'start' to begin exploration");
}

int stableRead(int pin) {
  int count0 = 0, count1 = 0;

  for (int i = 0; i < 5; i++) {
    if (digitalRead(pin) == 0) count0++;
    else count1++;
    delay(2);
  }

  return (count0 > count1) ? 0 : 1;
}

void readSensors() {
  int sumS0 = 0, sumS1 = 0, sumS2 = 0, sumS3 = 0, sumS4 = 0;
  int samples = 2;

  for (int i = 0; i < samples; i++) {
    sumS0 += digitalRead(S0);
    sumS1 += digitalRead(S1);
    sumS2 += digitalRead(S2);
    sumS3 += digitalRead(S3);
    sumS4 += digitalRead(S4);
    delay(1); 
  }

  LeftFar   = (sumS0 > samples / 2) ? 1 : 0;
  LeftNear  = (sumS1 > samples / 2) ? 1 : 0;
  Center    = (sumS2 > samples / 2) ? 1 : 0;
  RightNear = (sumS3 > samples / 2) ? 1 : 0;
  RightFar  = (sumS4 > samples / 2) ? 1 : 0;
}

void forward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, baseSpeed);
  analogWrite(ENB, baseSpeed);
}

void stopMotors() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

void pdControlFollow(int LN, int C, int RN) {
  static int lastError = 0;
  int error = 0;

  if      (LN == 0 && C == 1 && RN == 1) error = -2;      
  else if (LN == 0 && C == 0 && RN == 1) error = -1; 
  else if (LN == 1 && C == 0 && RN == 1) error = 0;  
  else if (LN == 1 && C == 0 && RN == 0) error = 1;  
  else if (LN == 1 && C == 1 && RN == 0) error = 2;  
  else error = lastError; 

  int derivative = error - lastError;
  int correction = (Kp * error) + (Kd * derivative);
  lastError = error;

  int leftSpeed = baseSpeed + correction; 
  int rightSpeed = baseSpeed - correction;

  leftSpeed = constrain(leftSpeed, 0, 255);
  rightSpeed = constrain(rightSpeed, 0, 255);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, leftSpeed);
  analogWrite(ENB, rightSpeed);
}

void turnLeft90() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  analogWrite(ENA, turnSpeed);
  analogWrite(ENB, turnSpeed);

  delay(250);
  while (stableRead(S2) == 1);

  stopMotors();
}

void turnRight90() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  analogWrite(ENA, turnSpeed);
  analogWrite(ENB, turnSpeed);

  delay(250);
  while (stableRead(S2) == 1);

  stopMotors();
}

void turn180() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  analogWrite(ENA, turnSpeed);
  analogWrite(ENB, turnSpeed);

  delay(500);
  while (stableRead(S2) == 1);

  stopMotors();
}

void updateLeft()  { currentDirection = (Direction)((currentDirection + 3) % 4); }
void updateRight() { currentDirection = (Direction)((currentDirection + 1) % 4); }
void updateBack()  { currentDirection = (Direction)((currentDirection + 2) % 4); }

void recordMove(char localMove) {
  if (mazeSolved) return;
  char globalMove = getGlobalHeading();
  path[pathLength++] = globalMove;
  path[pathLength] = '\0';
  Serial3.print("Local Action: ");
  Serial3.print(localMove);
  Serial3.print(" | Global Heading Recorded: ");
  Serial3.println(globalMove);
}

void optimizePath() {
  bool changed = true;
  while (changed) {
    changed = false;
    for (int i = 0; i < pathLength - 1; i++) {
      if ((path[i] == 'N' && path[i+1] == 'S') ||
          (path[i] == 'S' && path[i+1] == 'N') ||
          (path[i] == 'E' && path[i+1] == 'W') ||
          (path[i] == 'W' && path[i+1] == 'E')) {
        
        for (int j = i; j < pathLength - 2; j++) {
          path[j] = path[j+2];
        }
        pathLength -= 2;
        path[pathLength] = '\0'; 
        changed = true;
        break; 
      }
    }
  }
  Serial3.println("=== OPTIMIZED GLOBAL PATH ===");
  Serial3.println(path);
}

void runOptimizedPath() {
  readSensors(); 

  // ===== GOAL DETECTION (same as exploration) =====
  if (pathIndex >= pathLength && 
      LeftFar == 0 && LeftNear == 0 && Center == 0 && 
      RightNear == 0 && RightFar == 0) {
    
    forward();
    delay(50);

    unsigned long startTime = millis();
    while (millis() - startTime < 200) {
      readSensors();
      if (LeftFar != 0 || LeftNear != 0 || Center != 0 || 
          RightNear != 0 || RightFar != 0) {
        return;
      }
    }

    stopMotors();
    Serial3.println("=== OPTIMIZED MAZE SOLVED! ===");
    runOptimizedMode = false;
    pathIndex = 0;
    inIntersection = false;
    return;
  }

  // ===== INTERSECTION DETECTION (same as exploration) =====
  bool isJunction = (LeftFar == 0 || RightFar == 0);

  if (isJunction && !inIntersection) {
    
    // STEP 1: Check available paths (same as exploration)
    bool canTurnLeft = (LeftFar == 0);
    bool canTurnRight = (RightFar == 0);

    // STEP 2: Center into intersection (same as exploration)
    unsigned long centerStartTime = millis();
    forward();
    
    while (millis() - centerStartTime < 150) {
      readSensors();
      if (LeftFar == 0) canTurnLeft = true;
      if (RightFar == 0) canTurnRight = true;
    }
    stopMotors();

    // STEP 3: Check straight path (same as exploration)
    int straightPathExists = stableRead(S2);
    bool canGoStraight = (straightPathExists == 0);

    // STEP 4: Get target global direction from optimized path
    if (pathIndex < pathLength) {
      char targetGlobal = path[pathIndex];
      Direction targetDir = currentDirection;

      if (targetGlobal == 'N') targetDir = NORTH;
      else if (targetGlobal == 'E') targetDir = EAST;
      else if (targetGlobal == 'S') targetDir = SOUTH;
      else if (targetGlobal == 'W') targetDir = WEST;

      // Calculate which way to turn
      int turnDiff = (targetDir - currentDirection + 4) % 4;

      // STEP 5: Execute based on turnDiff, but verify path is available (same safety as exploration)
      if (turnDiff == 3 && canTurnLeft) {  // Left turn
        turnLeft90();
        updateLeft();
        Serial3.print("Turned LEFT at junction, now facing ");
        Serial3.println(getGlobalHeading());
        pathIndex++;
      }
      else if (turnDiff == 1 && canTurnRight) {  // Right turn
        turnRight90();
        updateRight();
        Serial3.print("Turned RIGHT at junction, now facing ");
        Serial3.println(getGlobalHeading());
        pathIndex++;
      }
      else if (turnDiff == 0 && canGoStraight) {  // Straight
        Serial3.println("Going STRAIGHT at junction");
        pathIndex++;
      }
      else if (turnDiff == 2) {  // U-turn
        turn180();
        updateBack();
        Serial3.println("Turned BACK at junction");
        pathIndex++;
      }
      else {
        // Path mismatch - fallback to exploration priority (Left > Straight > Right)
        Serial3.print("WARNING: Path wants ");
        Serial3.print(targetGlobal);
        Serial3.print(" but not available. Using exploration logic. Path index: ");
        Serial3.println(pathIndex);
        
        if (canTurnLeft) {
          turnLeft90();
          updateLeft();
          Serial3.println("Exploration fallback: Turning LEFT");
          pathIndex++;
        }
        else if (canGoStraight) {
          Serial3.println("Exploration fallback: Going STRAIGHT");
          pathIndex++;
        }
        else if (canTurnRight) {
          turnRight90();
          updateRight();
          Serial3.println("Exploration fallback: Turning RIGHT");
          pathIndex++;
        }
      }
    }
    
    inIntersection = true;
    
    Serial3.print("Path progress: ");
    Serial3.print(pathIndex);
    Serial3.print("/");
    Serial3.println(pathLength);
  }
  
  // ===== NORMAL LINE FOLLOWING (same as exploration) =====
  else if (Center == 0 || LeftNear == 0 || RightNear == 0) {
    pdControlFollow(LeftNear, Center, RightNear);
  }

  // ===== EXIT INTERSECTION CONDITION (same as exploration) =====
  if (Center == 0 && RightNear == 1 && LeftNear == 1 && 
      RightFar == 1 && LeftFar == 1) {
    inIntersection = false;
    Serial3.println("Exited intersection");
  }

  // ===== DEAD END DETECTION (same as exploration) =====
  if (LeftFar == 1 && LeftNear == 1 && Center == 1 && 
      RightNear == 1 && RightFar == 1) {
    
    unsigned long startTime = millis();
    bool isDeadEnd = true;

    while (millis() - startTime < 200) {
      readSensors();
      if (LeftFar == 0 || LeftNear == 0 || Center == 0 || 
          RightNear == 0 || RightFar == 0) {
        isDeadEnd = false;
        break;
      }
    }
    
    if (isDeadEnd) {
      turn180();
      updateBack();
      Serial3.println("Dead end detected in optimized run");
      return;
    }
  }
}

void loop() {
  if (Serial3.available() > 0) {
    String command = Serial3.readStringUntil('\n');
    command.trim();
    
    if (command.equalsIgnoreCase("start")) {
      if (!mazeSolved && !explorationStarted) {
        explorationStarted = true;
      }
    }
    else if (command.equalsIgnoreCase("optimized")) {
      if (mazeSolved) {
        runOptimizedMode = true;
        pathIndex = 0;
        currentDirection = startDirection;
        inIntersection = false;
        delay(1000);
      }
    }
    else if (command.equalsIgnoreCase("stop")) {
      stopMotors();
      runOptimizedMode = false;
      explorationStarted = false;
    }
    else if (command.equalsIgnoreCase("optimize")) {
      if (mazeSolved) {
        optimizePath();
      }
    }
  }

  if (mazeSolved && !runOptimizedMode) {
    return; 
  }

  if (runOptimizedMode) {
    runOptimizedPath();
    return;
  }

  if (!explorationStarted || mazeSolved) {
    return;
  }

  readSensors(); 

  if (LeftFar == 0 && LeftNear == 0 && Center == 0 && RightNear == 0 && RightFar == 0) {
    forward();
    delay(50);

    unsigned long startTime = millis();
    while (millis() - startTime < 200) {
      readSensors();
      if (LeftFar != 0 || LeftNear != 0 || Center != 0 || RightNear != 0 || RightFar != 0) {
        return; 
      }
    }

    stopMotors();
    mazeSolved = true;
    
    Serial3.println("=== MAZE SOLVED ===");
    Serial3.print("Raw Path: ");
    Serial3.println(path);
    optimizePath();
    Serial3.println("Type 'optimized' to run the fast path.");
    return; 
  }

  bool isJunction = (LeftFar == 0 || RightFar == 0);

  if (isJunction && !inIntersection) {
    
    bool canTurnLeft = (LeftFar == 0);
    bool canTurnRight = (RightFar == 0);

    unsigned long centerStartTime = millis();
    forward();
    
    while (millis() - centerStartTime < 150) {
      readSensors(); 
      if (LeftFar == 0) canTurnLeft = true;
      if (RightFar == 0) canTurnRight = true;
    }
    stopMotors();

    int straightPathExists = stableRead(S2);
    bool canGoStraight = (straightPathExists == 0);

    if (canTurnLeft) {
      turnLeft90();
      updateLeft();
      recordMove('L');
    }
    else if (canGoStraight) {
      recordMove('S');
    }
    else if (canTurnRight) {
      turnRight90();
      updateRight();
      recordMove('R');
    }
    
    inIntersection = true;
  }
  
  else if (Center == 0 || LeftNear == 0 || RightNear == 0) {
    pdControlFollow(LeftNear, Center, RightNear);
  }

  if (Center == 0 && RightNear == 1 && LeftNear == 1 && RightFar == 1 && LeftFar == 1) {
    inIntersection = false;
  }

  if (LeftFar == 1 && LeftNear == 1 && Center == 1 && RightNear == 1 && RightFar == 1) {
      
    unsigned long startTime = millis();
    bool isDeadEnd = true;

    while (millis() - startTime < 200) {
      readSensors();
      if (LeftFar == 0 || LeftNear == 0 || Center == 0 || RightNear == 0 || RightFar == 0) {
        isDeadEnd = false;
        break;
      }
    }
    
    if (isDeadEnd) {
      turn180();
      updateBack();
      recordMove('B');
      return;
    }
  }
}
