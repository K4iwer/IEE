#define analogInput 12 // entrada analogica do microfone
#define digitalInput 14 // entrada digital do microfone
#define tower1 32
#define tower2 27
#define tower3 33

#define towerAmount 3
#define delayBetweenTowers 500
#define calibrateAmount 20
#define calibrateTimeout 500
#define meanError 30
#define distanceFindTries 10
#define distanceTriesTimeout 100
#define timeDivider 100000
#define timeout 0

//positions
//tower 1
#define x1 0.0
#define y1 0.0

//tower 2
#define x2 19.0
#define y2 0.0

//tower 3
#define x3 12.5
#define y3 10.0

//position calculation
#define A 2*(x1 - x2)
#define B 2*(y1 - y2)
#define C 2*(x2 - x3)
#define D 2*(y2 - y3)

int towers[] = {tower1, tower2, tower3};
int lastTowerNum = 0;
int mean = 0;
int currentSound = 0;
int maxError = 0;

bool timerRunning = false;


hw_timer_t* myTimer;

void IRAM_ATTR onTimer()
{
  timerStop(myTimer);
  digitalWrite(towers[lastTowerNum], LOW);
  Serial.println("Ended");
  timerRunning = false;
}


void setup() {
  Serial.begin(115200); 

  pinMode(analogInput, INPUT);
  pinMode(digitalInput, INPUT);
  pinMode(tower1, OUTPUT);
  pinMode(tower2, OUTPUT);
  pinMode(tower3, OUTPUT);

  Serial.begin(115200);
  Serial.println("Starting...");
  delay(5000);
  calibrateMean();
 
  myTimer = timerBegin(0, 2, true);
  timerAttachInterrupt(myTimer, &onTimer, true);
  timerAlarmWrite(myTimer, 160000000, true);
  timerAlarmEnable(myTimer);
  timerStop(myTimer);

}

void loop() {

  if (Serial.available()) {  
    char msg = Serial.read();
    Serial.println(msg);
    if(msg == '1') // tenta achar as 3 distancias
    {

      double distances[3];

      findThreeDistances(&distances[0]);
      for(int i = 0; i < 3; i++)
      {
        Serial.print("Torre ");
        Serial.print(i+1);
        Serial.print(": ");
        Serial.print(distances[i]);
        Serial.println(" cm");
      }

      double position[2];
      calculatePosition(&distances[0], &position[0]);

      Serial.println("Positions: ");
      Serial.print("X = ");
      Serial.println(position[0]);
      Serial.print("Y = ");
      Serial.println(position[1]);

    }else if(msg == '2')
    {
      Serial.println("Starting calibration in 2 secs");
      delay(2000);
      calibrateMean();
    }else Serial.println(" ");
  }
 
  if(digitalRead(digitalInput))
  {
    currentSound = analogRead(analogInput);
    hasSound(currentSound);
    /*Serial.print("hasSound:");                // para testar o microfone
    Serial.println(hasSound(currentSound));
    Serial.print("soundLevel:");
    Serial.println(currentSound);
    Serial.print("mean:");
    Serial.println(mean);
    Serial.print("Error:");
    Serial.println(maxError);*/
    
  }

  delay(timeout);
}

void findThreeDistances(double *distanceArray)// tenta achar as 3 distancias
{
 
  for(int i = 0; i < towerAmount; i++)     
  {
    distanceArray[i] = 0;
    distanceArray[i] = findDistance(i);
    delay(delayBetweenTowers);
  }
}
 
void refreshMean(int entry) // média dinamica, só entra aqui se não detectar um som feito por buzzer, ou seja ruido
{
  mean += entry;
  mean /= 2;
}
 
void calibrateMean() // necessário de silencio extremo nessa parte
{                     //valores de maxError > 50 implicam q teve algum disturbio bem grande, recalibrar!
  currentSound = analogRead(analogInput);
  int pastSound = currentSound;
  maxError = 0;
  for(int i = 0; i < calibrateAmount; i++)
  {
    currentSound = analogRead(analogInput);
    if(abs(currentSound - pastSound) > maxError) maxError = abs(currentSound - pastSound);
    refreshMean(currentSound);
    Serial.print("Calibrating - ");
    Serial.print(i);
    Serial.print("/");
    Serial.println(calibrateAmount);
    delay(calibrateTimeout);
    pastSound = currentSound;
  }
  maxError += meanError;
  Serial.println("Done!");
  Serial.print("MaxError: ");
  Serial.println(maxError);
}
 
bool hasSound(int entry)
{
  if((entry > (mean + maxError)) || (entry < (mean - maxError))) return true;
 
  refreshMean(entry);
  return false;
}
 
double findDistance(int tower)
{
  uint64_t maxTime = 0;
  int tries = 0;
  Serial.println("Started finding distance.");
  uint64_t previousTime;

  lastTowerNum = tower;
  for(int i = 0; i < distanceFindTries; i++)
  {
    timerWrite(myTimer, 0);
    timerAlarmEnable(myTimer);
    timerRunning = true;

    digitalWrite(towers[tower], HIGH);
    //while(!SerialBT.available());
    timerStart(myTimer);
    
    while(!hasSound(analogRead(analogInput)) && timerRunning);

    if(timerRunning)
    {
      uint64_t endTime = timerReadMicros(myTimer);
      timerStop(myTimer);
      timerAlarmDisable(myTimer);
      
      timerRunning = false;
      digitalWrite(towers[tower], LOW);
      Serial.print("Ended with time variation: ");
      Serial.println(endTime);
      if(endTime > maxTime) maxTime = endTime;
      if(i==0) previousTime = endTime;
      

      if((abs(((int)(previousTime - endTime))) > 2000))
      {
        i =0;
        maxTime = 0;
        tries++;
        Serial.println("Sus! -> reset");
        if(tries >= 3)
        {
          Serial.println("kek");
          return findDistanceMean(tower);
        }
      }

      previousTime = endTime;
    }else{

      Serial.println("TimeOut");
      i = 0;
      maxTime = 0;
    }
    delay(distanceTriesTimeout);
  }


  if(microTimeToDistanceInCM(maxTime) < 11)
  {
    return microTimeToDistanceInCM(maxTime);
    //return microTimeToDistanceInCM(2*maxTime);
  }else return findDistanceMean(tower);
}

double findDistanceMean(int tower)
{
  uint64_t avarageTime = 0;
  int amountOfTimes = 0;
  Serial.println("Started finding distance mean.");
  uint64_t previousTime;

  lastTowerNum = tower;
  for(int i = 0; i < 2*distanceFindTries; i++)
  {
    timerWrite(myTimer, 0);
    timerAlarmEnable(myTimer);
    timerRunning = true;

    digitalWrite(towers[tower], HIGH);
    //while(!SerialBT.available());
    timerStart(myTimer);
    
    while(!hasSound(analogRead(analogInput)) && timerRunning);

    if(timerRunning)
    {
      uint64_t endTime = timerReadMicros(myTimer);
      timerStop(myTimer);
      timerAlarmDisable(myTimer);
      
      timerRunning = false;
      digitalWrite(towers[tower], LOW);
      Serial.print("Ended with time variation: ");
      Serial.println(endTime);

      avarageTime += endTime;
      amountOfTimes ++;
      //if(endTime > maxTime) maxTime = endTime;  
      if(i==0) previousTime = endTime;
      

      if((abs(((int)(previousTime - endTime))) > 5000))
      {
        i =0;
        avarageTime = 0;
        amountOfTimes = 0;

        Serial.println("Sus! -> reset");
      }

      previousTime = endTime;
    }else{

      Serial.println("TimeOut");
      i = 0;
      avarageTime = 0;
      amountOfTimes = 0;
    }
    delay(distanceTriesTimeout);
  }

  return microTimeToDistanceInCM(avarageTime/amountOfTimes);
  //return microTimeToDistanceInCM(2*avarageTime/amountOfTimes);
}

double microTimeToDistanceInCM(uint64_t timeVariation)
{
  return (timeVariation * 343.0)/timeDivider;
} 

void calculatePosition(double* distances, double* positions)
{
  double g1 = (distances[1]*distances[1]) + (x1*x1) + (y1*y1) - (distances[0]*distances[0]) - (x2*x2) - (y2*y2);
  double g2 = (distances[2]*distances[2]) + (x2*x2) + (y2*y2) - (x3*x3) - (y3*y3) - (distances[1]*distances[1]);

  Serial.print("A = ");
  Serial.println(A);
  
  Serial.print("B = ");
  Serial.println(B);
  
  Serial.print("C = ");
  Serial.println(C);
  
  Serial.print("D = ");
  Serial.println(D);

  Serial.print("g1 = ");
  Serial.println(g1);
  
  Serial.print("g2 = ");
  Serial.println(g2);
  positions[1] = ((A*g2) - (C*g1))/((A*D) - (B*C));
  if(A != 0){
    positions[0] = g1 - (B*positions[1]);
    positions[0] /= A;
  }else
  {
    positions[0] = g2 - (D*positions[1]);
    positions[0] /= C;
  }
}
