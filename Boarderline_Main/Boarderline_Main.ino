/*
   test directions of motors and speeds
   add limit switch
   connect limit switch and test calibration
   add 5v regulator and servo
*/

//Boarderline

#include <ESP32_Servo.h>
#include <WiFi.h>

// Don't forget to change this value to match your JSON document.
// Use arduinojson.org/assistant to compute the capacity.
/*
   Will expect this json blob
   {"coord":{"lon":-71.06,"lat":42.36},
   "weather":[{"id":803,"main":"Clouds","description":"broken clouds","icon":"04d"}],
   "base":"stations",
   "main":{"temp":296.23,"pressure":1015,"humidity":28,"temp_min":294.15,"temp_max":298.15},
   "visibility":16093,"wind":{"speed":4.6,"deg":250},"clouds":{"all":75},
   "dt":1525265760,
   "sys":{"type":1,"id":1274,"message":0.0065,"country":"US","sunrise":1525253841,"sunset":1525304739},
   "id":4930956,
   "name":"Boston",
   "cod":200}
*/

//things for wifi
const int response_timeout = 6000; //ms to wait for response from host

//helper getting function
String do_GET() {
  //check http://api.openweathermap.org/data/2.5/weather?q=Boston&appid=112e62793bece3cadd246dcbea62d7cc

  WiFiClient client; //instantiate a client object

  if (client.connect("api.openweathermap.org", 80)) { //try to connect to  host
    // This will send the request to the server

    // If connected, fire off HTTP GET:
    client.println("GET /data/2.5/weather?q=Boston&appid=112e62793bece3cadd246dcbea62d7cc HTTP/1.1");
    client.println("Host: api.openweathermap.org");
    client.print("\r\n");
    unsigned long count = millis();

    while (client.connected()) { //while we remain connected read out data coming back
      String line = client.readStringUntil('\n');
      Serial.println(line);
      if (line == "\r") { //found a blank line!
        //headers have been received! (indicated by blank line)
        break;
      }
      if (millis() - count > response_timeout) break;
    }
    count = millis();
    String op; //create empty String object
    while (client.available()) { //read out remaining text (body of response)
      op += (char)client.read();
    }
    //Serial.print(op);
    return op;

  }
}
int getWeather() {
  //TRIAL
  String temp = do_GET();
  Serial.println(temp);
  String kelvin = temp.substring(temp.indexOf("temp_max") + 10, temp.indexOf("temp_max") + 14);
  Serial.println("Temper Kelvin " + kelvin);
  int celsiusMax = kelvin.toInt() - 273;
  Serial.println("C: " + String(celsiusMax));
  return (celsiusMax);
}

class Boarderline {
  public:
    float angle = 0; // the ANGLE in degrees we are in right now
    float radius = 0; // the radius in cm we are in right now
    float xLast = 0;
    float yLast = 0;
    int rackPos = 0; //in steps. conversion: steps/210 = cm <==> 210 STEPS PER CM
    float stepsToCm = 1 / 210;
    int cmToSteps = 210; //multiply by me to get steps from needed cm movement
    float angleToSteps = 4.444; // multiply by me to get steps from needed angle movement
    float radiusMax = 20.00; // cm
    float radiusMin = 7; // cm
    int anglePos = 0;// in steps. conversion: steps/4.444 = degrees -- assuming we are at 1/8 steps, which means 200*8 steps in 360degs
    float angleMax =  136; // degrees
    float angleMin = 45; // degrees
    int stepA = 0; // for angle
    int dirA = 0;
    int stepR = 0; // for rack
    int dirR = 0;
    int limitPin = 2; // digital HIGH means pressed - changes in initialization
    int servoPin = 15;
    Servo markerServo;
    const bool CW = LOW;
    const bool CCW = HIGH;
    const bool in = HIGH;
    const bool out = LOW;
    unsigned long lastStepA = 0;
    unsigned long lastStepR = 0;
    int speedA = 10; // means the number of milliseconds it waits before stepping again
    int speedR = 3;

    //function to set up instant
    Boarderline(int inStepA, int inDirA, int inStepR, int inDirR, int inLimitPin, int inServoPin) {
      stepA = inStepA;
      dirA = inDirA;
      stepR = inStepR;
      dirR = inDirR;
      servoPin = inServoPin;
      limitPin = inLimitPin;

    }
    void initialize() {
      pinMode(stepA, OUTPUT);
      pinMode(dirA, OUTPUT);
      pinMode(stepR, OUTPUT);
      pinMode(dirR, OUTPUT);
      pinMode(limitPin, INPUT_PULLUP); //when button is pressed, digitalRead(limitPin) == LOW
      pinMode(servoPin, OUTPUT);
      markerServo.attach(15);
      marker(false);
    }
    void reset() {
      //for starters assume that the angle is 90 degrees
      //move the rack until it touches the limit switch
      digitalWrite(dirR, out);//direction outwards
      while (digitalRead(limitPin) == LOW) { // while button not pressed
        //as long as not touches, go another step towards the limit
        if (millis() - lastStepR >= speedR) { //make sure it moves in a reasonable speed
          Serial.println("Step");
          digitalWrite(stepR, HIGH);
          digitalWrite(stepR, LOW);
          lastStepR = millis();
        }
      }
      //now we got to full extension of rack
      radius = radiusMax; // in cm
      angle = 90; // degrees
      xLast = 0;
      yLast = radiusMax;
      delay(1000);
      marker(true);
      delay(100);
      marker(false);
    }
    void to(float inX, float inY) { //expects coordinates in cm
      //convert to target angle and target radius
      float radiusT = sqrt(pow(inX, 2) + pow(inY, 2));
      float angleT = atan(inY / inX) * 360 / (2 * PI);
      if (angleT < 0) {
        angleT += 180;
      }
      //Serial.print("Going Angle: "); Serial.println(angleT);
      //Serial.print("Going Radius: "); Serial.println(radiusT);
      //check if in range
      if (angleT < angleMin || angleT > angleMax) {
        Serial.println("angle out of range");
      }
      else if (radiusT > radiusMax || radiusT < radiusMin) {
        Serial.println("Radius out of range");
      }
      else {
        // what direction should we move in
        bool radiusDir;
        bool angleDir;
        //direction of angle
        if (angleT > angle) {
          angleDir =  CCW;
        }
        else if (angleT < angle) {
          angleDir = CW;
        }
        float angleDelta = fabs(angleT - angle); // <=== that's what we need for later. we now have the number of degrees and direction
        //Serial.print("moving degrees: "); Serial.println(angleDelta);
        //direction of radius
        if (radiusT > radius) {
          radiusDir = out;
        }
        else if (radiusT < radius) {
          radiusDir = in;
        }
        float radiusDelta = fabs(radiusT - radius);// <=== that's what we need for later. we now have the number of cm and direction
        //Serial.print("moving cm: "); Serial.println(radiusDelta);
        //convert the movement to steps
        int angleDeltaSteps = angleDelta * angleToSteps;
        int radiusDeltaSteps = radiusDelta * cmToSteps;
        //print the steps
        //Serial.print("Moving Angle Steps: "); Serial.println(angleDeltaSteps);
        //Serial.print("Moving Radius Steps: "); Serial.println(radiusDeltaSteps);
        //now we can actually start moving
        bool radiusThere = false;
        bool angleThere = false;
        int radiusCounter = 0;
        int angleCounter = 0;
        bool allThere = false;
        while (allThere == false) {
          //check if we have gone enough steps
          if (radiusCounter >= radiusDeltaSteps) {
            radiusThere = true;
          }
          if (angleCounter >= angleDeltaSteps) {
            angleThere = true;
          }
          if (radiusThere == true) {
            if (angleThere == true) {
              allThere = true;
            }
          }
          //move angle (and radius with it so we won't lose distance)
          if (angleThere == false) {
            if (millis() - lastStepA >= speedA) {
              digitalWrite(dirA, angleDir);
              digitalWrite(dirR, !angleDir);
              digitalWrite(stepA, HIGH);
              digitalWrite(stepA, LOW);
              digitalWrite(stepR, HIGH);
              digitalWrite(stepR, LOW);
              //set times for last step
              lastStepA = millis();
              //add to counter
              angleCounter += 1;
              //Serial.print("angleCounter = "); Serial.println(angleCounter);
            }
          }
          //move radius
          if (radiusThere == false) {
            if (millis() - lastStepR >= speedR) {
              digitalWrite(dirR, radiusDir);
              digitalWrite(stepR, HIGH);
              digitalWrite(stepR, LOW);
              //set time for last step
              lastStepR = millis();
              //add to counter
              radiusCounter += 1;
              //Serial.print("RadiusCounter = "); Serial.println(radiusCounter);
            }
          }
        }
        //we are out of the while loop - we got to our destination!
        //update the global radius and angle variables
        if (radiusDir == out) {
          radius += radiusDelta;
        }
        else if (radiusDir == in) {
          radius -= radiusDelta;
        }
        if (angleDir == CCW) {
          angle += angleDelta;
        }
        else if (angleDir == CW) {
          angle -= angleDelta;
        }
        //set last values
        xLast = inX;
        yLast = inY;
      }
    }
    void marker(bool touch) {
      if (touch == true) {
        markerServo.write(77);
      }
      if (touch == false) {
        markerServo.write(60);
      }
    }
    void toLine(float xt, float yt, float delta = 0.2) {//delta is the "step" value, in cm, defaulted to 0.1cm
      delta = fabs(delta); // make sure it's positive
      //get cartesian coordinates from angle and radius
      float xi = xLast;//radius * cos(angle / 180 * PI);
      float yi = yLast;//radius * sin(angle / 180 * PI);
      if (xi == xt) {
        //means we have a vertical line - decide on direction of motion
        if (yt < yi) {
          delta *= -1;
        }
        //calculate number of parts ("steps") we can go through
        float parts = fabs(yt - yi) / fabs(delta);
        int partsInt = int(parts);
        //Serial.print("have vert line, with parts = "); Serial.println(partsInt);
        //go through the parts
        for (int i = 0; i < partsInt; i++) {
          to(xt, yi + i * delta);
        }
        to(xt, yt);
      }
      else { // if xi != xt then we are not on a vertical line
        float m =  (yt - yi) / (xt - xi); // claculate slope of line
        if (xt < xi) {
          delta *= -1;
        }
        //calculates number of parts we can walk
        float parts = fabs(xt - xi) / fabs(delta);
        int partsInt = int(parts);
        //go
        for (int i = 0; i < partsInt; i++) {
          float yPart = m * (i * delta) + yi; // claculate y for given x = [xi + i*delta] on line. xi vanishes from substraction
          //Serial.print("GOING TO PART WITH X = "); Serial.print(xi + i * delta); Serial.print(" AND Y = "); Serial.println(yPart);
          to(xi + i * delta, yPart);
        }
        to(xt, yt);//the to function takes care of updating global variables x/yLast and radius and angle
      }
    }
    void printChar(char inChar, int index) {
      int xCoords[] = { -8, -6, -4, -2, 0, 2, 4, 6,
                        -8, -6, -4, -2, 0, 2, 4, 6,
                        -8, -6, -4, -2, 0, 2, 4, 6,
                        -8, -6, -4, -2, 0, 2, 4, 6,
                        -8, -6, -4, -2, 0, 2, 4, 6
                      };
      int yCoords[] = { 16, 16, 16, 16, 16, 16, 16, 16,
                        14, 14, 14, 14, 14, 14, 14, 14,
                        12, 12, 12, 12, 12, 12, 12, 12,
                        10, 10, 10, 10, 10, 10, 10, 10,
                        8, 8, 8, 8, 8, 8, 8, 8
                      };
      switch (inChar) {
        case ' ':
          printSpace(xCoords[index], yCoords[index]);
          break;
        case 'A':
          printA(xCoords[index], yCoords[index]);
          break;
        case 'B':
          printB(xCoords[index], yCoords[index]);
          break;
        case 'C':
          printC(xCoords[index], yCoords[index]);
          break;
        case 'D':
          printD(xCoords[index], yCoords[index]);
          break;
        case 'E':
          printE(xCoords[index], yCoords[index]);
          break;
        case 'F':
          printF(xCoords[index], yCoords[index]);
          break;
        case 'G':
          printG(xCoords[index], yCoords[index]);
          break;
        case 'H':
          printH(xCoords[index], yCoords[index]);
          break;
        case 'I':
          printI(xCoords[index], yCoords[index]);
          break;
        case 'J':
          printJ(xCoords[index], yCoords[index]);
          break;
        case 'K':
          printK(xCoords[index], yCoords[index]);
          break;
        case 'L':
          printL(xCoords[index], yCoords[index]);
          break;
        case 'M':
          printM(xCoords[index], yCoords[index]);
          break;
        case 'N':
          printN(xCoords[index], yCoords[index]);
          break;
        case 'O':
          printO(xCoords[index], yCoords[index]);
          break;
        case 'P':
          printP(xCoords[index], yCoords[index]);
          break;
        case 'Q':
          printQ(xCoords[index], yCoords[index]);
          break;
        case 'R':
          printR(xCoords[index], yCoords[index]);
          break;
        case 'S':
          printS(xCoords[index], yCoords[index]);
          break;
        case 'T':
          printT(xCoords[index], yCoords[index]);
          break;
        case 'U':
          printU(xCoords[index], yCoords[index]);
          break;
        case 'V':
          printV(xCoords[index], yCoords[index]);
          break;
        case 'W':
          printW(xCoords[index], yCoords[index]);
          break;
        case 'X':
          printX(xCoords[index], yCoords[index]);
          break;
        case 'Y':
          printY(xCoords[index], yCoords[index]);
          break;
        case 'Z':
          printZ(xCoords[index], yCoords[index]);
          break;
        case '0':
          print0(xCoords[index], yCoords[index]);
          break;
        case '1':
          print1(xCoords[index], yCoords[index]);
          break;
        case '2':
          print2(xCoords[index], yCoords[index]);
          break;
        case '3':
          print3(xCoords[index], yCoords[index]);
          break;
        case '4':
          print4(xCoords[index], yCoords[index]);
          break;
        case '5':
          print5(xCoords[index], yCoords[index]);
          break;
        case '6':
          print6(xCoords[index], yCoords[index]);
          break;
          //        case '7':
          //          print7(xCoords[index], yCoords[index]);
          //          break;
          //        case '8':
          //          print8(xCoords[index], yCoords[index]);
          //          break;
          //        case '9':
          //          print9(xCoords[index], yCoords[index]);
          //          break;

      }
    }

    //list all individual funcions for writing letters
    void printSpace(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      //to(1 + xs, 1 + ys);
      //marker(true);
      //to(1.1 + xs, 1 + ys);
      //marker(false);
    }
    void printA(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(0.5 + xs, 0.25 + ys);
      marker(true);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(1.5 + xs, 1.75 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      toLine(1.5 + xs, 1 + ys);
      toLine(0.5 + xs, 1 + ys);
      marker(false);
    }
    void printB(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(0.5 + xs, 0.25 + ys);//bottom left
      marker(true);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(1 + xs, 1.75 + ys);
      toLine(1 + xs, 1 + ys);
      toLine(1.5 + xs, 1 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      toLine(0.5 + xs, 0.25 + ys);
      toLine(0.5 + xs, 1 + ys);
      toLine(1.5 + xs, 1 + ys);
      marker(false);
    }
    void printC(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(1.5 + xs, 1.75 + ys);//bottom left
      marker(true);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(0.5 + xs, 0.25 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      marker(false);
    }
    void printD(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(0.5 + xs, 0.25 + ys);
      marker(true);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(1 + xs, 1.75 + ys);
      toLine(1.5 + xs, 1.25 + ys);
      toLine(1.5 + xs, 0.75 + ys);
      toLine(0.5 + xs, 0.25 + ys);
      marker(false);
    }
    void printE(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(1.5 + xs, 1.75 + ys);
      marker(true);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(0.5 + xs, 1 + ys);
      toLine(1.5 + xs, 1 + ys);
      toLine(0.5 + xs, 1 + ys);
      toLine(0.5 + xs, 0.25 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      marker(false);
    }
    void printF(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(1.5 + xs, 1.75 + ys);
      marker(true);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(0.5 + xs, 1 + ys);
      toLine(1.5 + xs, 1 + ys);
      toLine(0.5 + xs, 1 + ys);
      toLine(0.5 + xs, 0.25 + ys);
      marker(false);
    }
    void printG(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(1.5 + xs, 1.75 + ys);
      marker(true);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(0.5 + xs, 0.25 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      toLine(1.5 + xs, 1 + ys);
      toLine(1 + xs, 1 + ys);
      marker(false);
    }
    void printH(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(0.5 + xs, 0.25 + ys);
      marker(true);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(0.5 + xs, 1 + ys);
      toLine(1.5 + xs, 1 + ys);
      toLine(1.5 + xs, 1.75 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      marker(false);
    }
    void printI(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(0.25 + xs, 0.25 + ys);
      marker(true);
      toLine(1.75 + xs, 0.25 + ys);
      toLine(1 + xs, 0.25 + ys);
      toLine(1 + xs, 1.75 + ys);
      toLine(0.25 + xs, 1.75 + ys);
      toLine(1.75 + xs, 1.75 + ys);
      marker(false);
    }
    void printJ(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(0.5 + xs, 0.25 + ys);
      marker(true);
      toLine(1 + xs, 0.25 + ys);
      toLine(1 + xs, 1.75 + ys);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(1.5 + xs, 1.75 + ys);
      marker(false);
    }
    void printK(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(0.5 + xs, 0.25 + ys);
      marker(true);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(0.5 + xs, 1.25 + ys);
      toLine(1.5 + xs, 1.75 + ys);
      toLine(0.75 + xs, 1.25 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      marker(false);
    }
    void printL(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(0.5 + xs, 1.75 + ys);
      marker(true);
      toLine(0.5 + xs, 0.25 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      marker(false);
    }
    void printM(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(0.5 + xs, 0.25 + ys);
      marker(true);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(1 + xs, 1.5 + ys);
      toLine(1.5 + xs, 1.75 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      marker(false);
    }
    void printN(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(0.5 + xs, 0.25 + ys);
      marker(true);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      toLine(1.5 + xs, 1.75 + ys);
      marker(false);
    }
    void printO(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(0.5 + xs, 0.25 + ys);
      marker(true);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(1.5 + xs, 1.75 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      toLine(0.5 + xs, 0.25 + ys);
      marker(false);
    }
    void printP(int xs, int ys) {//x start and y start - coordinates of bottom left corner of letter
      marker(false);
      to(0.5 + xs, 1 + ys);
      marker(true);
      toLine(1.5 + xs, 1 + ys);
      toLine(1.5 + xs, 1.75 + ys);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(0.5 + xs, 0.25 + ys);
      marker(false);
    }
    void printQ(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(0.5 + xs, 0.25 + ys);
      marker(true);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(1.5 + xs, 1.75 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      toLine(0.5 + xs, 0.25 + ys);
      toLine(1 + xs, 0.4 + ys);
      toLine(1.7 + xs, 0 + ys);
      marker(false);
    }
    void printR(int xs, int ys) {//x start and y start - coordinates of bottom left corner of letter
      marker(false);
      to(0.5 + xs, 0.25 + ys);
      marker(true);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(1.5 + xs, 1.75 + ys);
      toLine(1.5 + xs, 1 + ys);
      toLine(0.5 + xs, 1 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      marker(false);
    }
    void printS(int xs, int ys) {//x start and y start - coordinates of bottom left corner of letter
      marker(false);
      to(1.5 + xs, 1.75 + ys);
      marker(true);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(0.5 + xs, 1 + ys);
      toLine(1.5 + xs, 1 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      toLine(0.5 + xs, 0.25 + ys);
      marker(false);
    }
    void printT(int xs, int ys) {//x start and y start - coordinates of bottom left corner of letter
      marker(false);
      to(0.25 + xs, 1.75 + ys);
      marker(true);
      toLine(1.75 + xs, 1.75 + ys);
      toLine(1 + xs, 1.75 + ys);
      toLine(1 + xs, 0.25 + ys);
      marker(false);
    }
    void printU(int xs, int ys) {//x start and y start - coordinates of bottom left corner of letter
      marker(false);
      to(0.5 + xs, 1.75 + ys);
      marker(true);
      toLine(0.5 + xs, 0.25 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      toLine(1.5 + xs, 1.75 + ys);
      marker(false);
    }
    void printV(int xs, int ys) {//x start and y start - coordinates of bottom left corner of letter
      marker(false);
      to(0.5 + xs, 1.75 + ys);
      marker(true);
      toLine(0.9 + xs, 0.25 + ys);
      toLine(1.1 + xs, 0.25 + ys);
      toLine(1.5 + xs, 1.75 + ys);
      marker(false);
    }
    void printW(int xs, int ys) {//x start and y start - coordinates of bottom left corner of letter
      marker(false);
      to(0.5 + xs, 1.75 + ys);
      marker(true);
      toLine(0.5 + xs, 0.25 + ys);
      toLine(1 + xs, 1 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      toLine(1.5 + xs, 1.75 + ys);
      marker(false);
    }
    void printX(int xs, int ys) {//x start and y start - coordinates of bottom left corner of letter
      marker(false);
      to(0.5 + xs, 1.75 + ys);
      marker(true);
      toLine(1.5 + xs, 0.25 + ys);
      marker(false);
      to(1.5 + xs, 1.75 + ys);
      marker(true);
      toLine(0.5 + xs, 0.25 + ys);
      marker(false);
    }
    void printY(int xs, int ys) {//x start and y start - coordinates of bottom left corner of letter
      marker(false);
      to(0.5 + xs, 1.75 + ys);
      marker(true);
      toLine(1 + xs, 1 + ys);
      toLine(1.5 + xs, 1.75 + ys);
      toLine(1 + xs, 1 + ys);
      toLine(1 + xs, 0.25 + ys);
      marker(false);
    }
    void printZ(int xs, int ys) {//x start and y start - coordinates of bottom left corner of letter
      marker(false);
      to(0.5 + xs, 1.75 + ys);
      marker(true);
      toLine(1.5 + xs, 1.75 + ys);
      toLine(0.5 + xs, 0.25 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      marker(false);
    }
    void print0(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(0.5 + xs, 0.25 + ys);
      marker(true);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(1.5 + xs, 1.75 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      toLine(0.5 + xs, 0.25 + ys);
      toLine(1.5 + xs, 1.75 + ys);
      marker(false);
    }
    void print1(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(1 + xs, 1.75 + ys);
      marker(true);
      toLine(1 + xs, 0.25 + ys);
      marker(false);
    }
    void print2(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(0.5 + xs, 1.75 + ys);
      marker(true);
      toLine(1.5 + xs, 1.75 + ys);
      toLine(1.5 + xs, 1 + ys);
      toLine(0.5 + xs, 1 + ys);
      toLine(0.5 + xs, 0.25 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      marker(false);
    }
    void print3(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(0.5 + xs, 1.75 + ys);
      marker(true);
      toLine(1.5 + xs, 1.75 + ys);
      toLine(1.5 + xs, 1 + ys);
      toLine(0.5 + xs, 1 + ys);
      toLine(1.5 + xs, 1 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      toLine(0.5 + xs, 0.25 + ys);
      marker(false);
    }
    void print4(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(0.5 + xs, 1.75 + ys);
      marker(true);
      toLine(0.5 + xs, 1 + ys);
      toLine(1.5 + xs, 1 + ys);
      toLine(1.5 + xs, 1.75 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      marker(false);
    }
    void print5(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(1.5 + xs, 1.75 + ys);
      marker(true);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(0.5 + xs, 1 + ys);
      toLine(1.5 + xs, 1 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      toLine(0.5 + xs, 0.25 + ys);
      marker(false);
    }
    void print6(int xs, int ys) {//x start and y start - bottom left corner of letter
      marker(false);
      to(1.5 + xs, 1.75 + ys);
      marker(true);
      toLine(0.5 + xs, 1.75 + ys);
      toLine(0.5 + xs, 0.25 + ys);
      toLine(1.5 + xs, 0.25 + ys);
      toLine(1.5 + xs, 1 + ys);
      toLine(0.5 + xs, 1 + ys);
      marker(false);
    }
    void printString(String inString, int start = 0) {
      char stringArray[inString.length() + 1];
      inString.toCharArray(stringArray, inString.length()+1); //put string in array of chars we can use
      Serial.println("Our String is=" + inString + "--- Size of array=" + String(inString.length()));
      for (int i = 0; i < inString.length(); i++) {
        Serial.println("Writing: " + String(stringArray[i]) + " In Position: " + String(start + i));
        printChar(stringArray[i], start + i);
      }
    }
};


//++++++++++ NOW THE ACTUAL CODE STARTS ++++++++++++++++++//

Boarderline board(17, 16, 18, 5, 2, 9); //for rack: dir=5, step = 18, for angle: dir = 16, step = 17 | limit = 2, servo = 9

void setup() {
  Serial.begin(115200);
  board.initialize();
  //initiate wifi
  delay(100); //wait a bit (100 ms)
  WiFi.begin("Red Lights District", "omgrobots"); //attempt to connect to wifi
  int count = 0; //count used for Wifi check times
  while (WiFi.status() != WL_CONNECTED && count < 6) {
    delay(500);
    Serial.print(".");
    count++;
  }
  delay(2000);
  if (WiFi.isConnected()) { //if we connected then print our IP, Mac, and SSID we're on
    Serial.println(WiFi.localIP().toString() + " (" + WiFi.macAddress() + ") (" + WiFi.SSID() + ")");
    delay(1000);
  } else { //if we failed to connect just try again.
    Serial.println(WiFi.status());
    ESP.restart(); // restart the ESP
  }
  //start hardware
  delay(1000);
  Serial.print("Reseting Radius to max extention");
  board.reset();
  Serial.println("Radius at Max");


}

void loop() {

  board.printString("TOP", 2);
  //board.printString("TEMP", 10);
  board.printString(String(getWeather()), 18);
  //board.printString("TAKING", 24);
  //board.printString("OVER", 35);
  board.to(8, 8);
  //Serial.println("getWeather: " + String(getWeather()));
  delay(10000);

  board.marker(false);

  /*
    //MIT
    board.to(-2, 15);
    board.marker(true);
    board.to(-2, 17);
    board.to(-0.75, 17);
    board.to(-1.375, 17);
    board.to(-1.375, 15);
    board.to(-1.375, 17);
    board.to(-0.75, 17);
    board.to(-0.75, 15);


    board.marker(false);
    board.to(0, 15);
    board.marker(true);
    board.to(0, 17);

    board.marker(false);
    board.to(0.75, 17);
    board.marker(true);
    board.to(2.5, 17);
    //board.marker(false);
    board.to(1.375, 17);
    //board.marker(true);
    board.to(1.375, 15);
    board.marker(false);
  */

}
