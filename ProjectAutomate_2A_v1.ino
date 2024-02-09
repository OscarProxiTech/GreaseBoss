  // Pin definitions for motor control and the Hall effect sensor
  const int HAL_PIN = 3; // Pin connected to Hall effect sensor
  const int DIR_PIN = 4; // Pin for controlling motor direction
  const int PWM_PIN = 5; // Pin for controlling motor speed

  // Test parameters initialized with default values
  int testDir = 1; // Motor direction: 1 for forward, -1 for backward
  int testSpeed = 0; // Motor speed as PWM value (0-255)
  int testSpeedPec = 0; // Motor speed as input value (0-100)
  float kFactor = 1.0; // Constant factor for calculations
  float flowRate = 0.0; // Total test flow duration

  // Timing and flow variables
  volatile float flowCount = 0.0; // Edge counts from Hall sensor

  // Global variables for timing
  unsigned long testStartTime = 0;  // Start time of the test
  unsigned long testElapsedTime = 0;  // Elapsed time of the test
  unsigned long pauseStartTime = 0;  // Time when the test was paused
  unsigned long lastReportTime = 0;  // The last time the progress was reported

  // State machine for test control
  enum State { INACTIVE, IDLE, PLAY, PAUSE };
  State currentState = INACTIVE;

  // Function prototypes for test control
  void Con();
  void Dis();
  void TestStart();
  void TestEnd();
  void TestStop();
  void TestPause();
  void TestResume();

  void setup() {
    pinMode(HAL_PIN, INPUT);
    pinMode(DIR_PIN, OUTPUT);
    pinMode(PWM_PIN, OUTPUT);
    resetVar();
    Serial.begin(9600); // Initialize serial communication
    delay(100);
    attachInterrupt(digitalPinToInterrupt(HAL_PIN), hallEffectSensorISR, RISING); // Attach interrupt for Hall sensor
  }

void loop() {
  // State machine handling
  switch (currentState) {
    case INACTIVE:
      // Waiting for connection
      break;
    case IDLE:
      // Waiting for test start or disconnect
      break;
    case PLAY:
      if (flowCount * kFactor >= flowRate) {
        TestEnd(); // End test if duration has elapsed
      } else {
        unsigned long currentTime = millis();
        if (currentTime - lastReportTime >= 100) {  // Check if 0.25 seconds have passed
          reportProgress();
          lastReportTime = currentTime;
        }
      }
      break;
    case PAUSE:
      // Test is paused, awaiting resume or stop
      break;
  }
    // Handle incoming serial data
    if (Serial.available() > 0) {
      String incomingData = Serial.readStringUntil('\n');
      if (incomingData.length() == 1) { // Single character commands
        char command = incomingData.charAt(0);
        switch (command) {
          case 'c': Con(); break;
          case 'd': Dis(); break;
          case 's': if (currentState == IDLE || currentState == PAUSE) TestStart(); break;
          case 'p': if (currentState == PLAY) TestPause(); break;
          case 't': if (currentState == PLAY || currentState == PAUSE) TestEnd(); break;
        }
      } else { // CSV string for test parameters
        if (currentState == IDLE || currentState == PAUSE) parseTestParameters(incomingData);
      }
    }
}

// Reset variables to default states
void resetVar() {
  testDir = 1;
  testSpeed = 0;
  testSpeedPec = 0;
  flowRate = 0;
  flowCount = 0;
  testStartTime = 0;  // Start time of the test
  testElapsedTime = 0;  // Elapsed time of the test
  pauseStartTime = 0;  // Time when the test was paused
  analogWrite(PWM_PIN, testSpeed);
  setMotorDirection(testDir);
}

// Parse incoming CSV string to extract test parameters
void parseTestParameters(String data) {
  int firstCommaIndex = data.indexOf(',');
  int secondCommaIndex = data.indexOf(',', firstCommaIndex + 1);
  int thirdCommaIndex = data.indexOf(',', secondCommaIndex + 1);
  if (firstCommaIndex != -1 && secondCommaIndex != -1 && thirdCommaIndex != -1) {
    testDir = data.substring(0, firstCommaIndex).toInt();
    testSpeedPec = data.substring(firstCommaIndex + 1, secondCommaIndex).toInt();
    kFactor = data.substring(secondCommaIndex + 1, thirdCommaIndex).toFloat();
    flowRate = data.substring(thirdCommaIndex + 1).toInt();
    testSpeed = map(testSpeedPec, 0, 100, 0, 255); // Map percentage to PWM value
  } else {
    delay(100);
    Serial.println("Error: Invalid parameter string");
  }
}

// Send the current pluse, flow, and time to the script
void reportProgress() {
  unsigned long currentElapsedTime = millis() - testStartTime;
  unsigned long elapsedTimeInSeconds = currentElapsedTime / 1000;
  String message = "|Pulses: " + String(flowCount) + " - " + String(flowCount * kFactor) + "/" + String(flowRate) + "cc - Time: " + String(elapsedTimeInSeconds) + "|##";
  Serial.println(message);
}

// Set the motor direction based on the testDir variable
void setMotorDirection(int dir) {
  digitalWrite(DIR_PIN, dir == 1 ? HIGH : LOW);
}

// Interrupt service routine for the Hall effect sensor
void hallEffectSensorISR() {
  if (currentState == PLAY) {
    flowCount++; // Increment edge count on each sensor trigger only when test is running
  }
}

// Connection established
void Con() {
  currentState = IDLE;
  delay(100);
  Serial.println("Connected##");
}

  // Disconnect and reset the system
void Dis() {
  resetVar();
  detachInterrupt(digitalPinToInterrupt(HAL_PIN));
  currentState = INACTIVE;
  delay(100);
  Serial.println("Disconnected##");
}

// Start or resume the test
void TestStart() {
  delay(100);
  if (testSpeed > 0 && flowRate > 0) {
    Serial.println("Test started##");
    delay(100);
    setMotorDirection(testDir);
    analogWrite(PWM_PIN, testSpeed);
    if (currentState == IDLE) { // Start the timer only if the test is starting from IDLE state
      testStartTime = millis();
    } else if (currentState == PAUSE) { // If resuming from PAUSE, update the start time to account for paused duration
      testStartTime += millis() - pauseStartTime;
    }
    currentState = PLAY;
  } else {
    currentState = IDLE;
    delay(100);
    Serial.println("Error: Invalid test parameters##");
  }
}

// End the test and report results
void TestEnd() {
  analogWrite(PWM_PIN, 0);
  testElapsedTime = millis() - testStartTime; // Calculate total elapsed time of the test
  delay(100);
  String results = "<" + String(testDir) + "," + String(testSpeedPec) + "," + String(kFactor, 7) + "," + String(flowCount) + "," + String(flowCount * kFactor) + "," + String(testElapsedTime / 1000) + ">##";
  Serial.println(results);
  resetVar();
  currentState = IDLE;
  delay(100);
  Serial.println("Test finished##");
}

// Pause the test
void TestPause() {
  analogWrite(PWM_PIN, 0); // Stop the motor
  pauseStartTime = millis(); // Record the time when the test is paused
  currentState = PAUSE;
  delay(100);
  Serial.println("Test paused##");
}

// Resume the test after a pause
void TestResume() {
  // No need to change direction or speed, just resume the test
  analogWrite(PWM_PIN, testSpeed);
  currentState = PLAY;
  delay(100);
  Serial.println("Test resumed##");
}