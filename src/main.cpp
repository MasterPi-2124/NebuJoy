#include <Adafruit_TinyUSB.h>
#include <USB.h>
using namespace std;

#define INITIAL_DEADZONE 10

class Joystick {
public:
    Joystick(int xPin, int yPin, bool invX = false, bool invY = false)
        : xPin(xPin), yPin(yPin), filterIndex(0), invX(invX), invY(invY), xValue(0), yValue(0),
          centerPosX(0), centerPosY(0) {
        pinMode(xPin, INPUT);
        pinMode(yPin, INPUT);
        xValues.fill(0);
        yValues.fill(0);
    }

    void readData() {
        int x = analogRead(xPin);
        int y = analogRead(yPin);

        xValues[filterIndex] = x;
        yValues[filterIndex] = y;

        filterIndex = (filterIndex + 1) % FILTER_SIZE;

        xValue = average(xValues);
        yValue = average(yValues);

        if (invX == true) xValue = -xValue;
        if (invY == true) yValue = -yValue;
    }

    void setCenter() {
        int totalX = 0;
        int totalY = 0;

        for (int i = 0; i < CALIBRATION_READS; i++) {
            readData();
            totalX += xValue;
            totalY += yValue;
            delay(10);  // Delay between reads
        }

        centerPosX = totalX / CALIBRATION_READS;
        centerPosY = totalY / CALIBRATION_READS;
    }

    int getX() const {
        return xValue - centerPosX;
    }

    int getY() const {
        return yValue - centerPosY;
    }

private:
    int xPin;
    int yPin;
    int xValue;
    int yValue;
    int centerPosX;
    int centerPosY;
    int filterIndex;
    static const int FILTER_SIZE = 10;
    static const int CALIBRATION_READS = 50;
    std::array<int, FILTER_SIZE> xValues;
    std::array<int, FILTER_SIZE> yValues;
    bool invX;
    bool invY;

    int average(const std::array<int, FILTER_SIZE>& values) const {
        long sum = 0;
        for (int value : values) {
            sum += value;
        }
        return sum / FILTER_SIZE;
    }
};

class Motion {
public:
    Motion(int16_t transX = 0, int16_t transY = 0, int16_t transZ = 0, int16_t rotX = 0, int16_t rotY = 0, int16_t rotZ = 0) 
        : transX(transX), transY(transY), transZ(transZ), rotX(rotX), rotY(rotY), rotZ(rotZ) {}

    void applySpeed(int16_t speed) {
        transX = transX * speed / 100;
        transY = transY * speed / 100;
        transZ = transZ * speed / 100;
        rotX = rotX * speed / 100;
        rotY = rotY * speed / 100;
        rotZ = rotZ * speed / 100;
    }

    void log() const {
        Serial.print("Motion - Trans: X=");
        Serial.print(transX);
        Serial.print(" Y=");
        Serial.print(transY);
        Serial.print(" Z=");
        Serial.print(transZ);
        Serial.print(" | Rot: X=");
        Serial.print(rotX);
        Serial.print(" Y=");
        Serial.print(rotY);
        Serial.print(" Z=");
        Serial.println(rotZ);
    }

    int16_t transX;
    int16_t transY;
    int16_t transZ;
    int16_t rotX;
    int16_t rotY;
    int16_t rotZ;
};

class JoystickArray {
public:
    JoystickArray(std::array<Joystick, 4>& joysticks) : joysticks(joysticks), deadzone(INITIAL_DEADZONE), lastActivityTime(0) {}

    void calibrateDeadzone() {
        int16_t maxDeviation = 0;
        for (auto& joystick : joysticks) {
            joystick.readData();
            int deviationX = abs(joystick.getX());
            int deviationY = abs(joystick.getY());
            if (deviationX > maxDeviation) maxDeviation = deviationX;
            if (deviationY > maxDeviation) maxDeviation = deviationY;
        }
        deadzone = maxDeviation + 10; // Adding a buffer to ensure deadzone
        Serial.print("Calibrated deadzone: ");
        Serial.println(deadzone);
    }

    Motion calculateMotion() {
        std::array<int16_t, 8> currentReads;
        bool isCentered = true;

        for (int i = 0; i < 4; i++) {
            joysticks[i].readData();
            currentReads[2 * i] = joysticks[i].getX();
            currentReads[2 * i + 1] = joysticks[i].getY();

            if (abs(currentReads[2 * i]) > deadzone || abs(currentReads[2 * i + 1]) > deadzone) {
                isCentered = false;
            }
        }

        if (isCentered) {
            return Motion();
        }

        Motion motion;
        motion.transX = calculateTranslation(currentReads, 0, 2);
        motion.transY = calculateTranslation(currentReads, 3, 6);
        motion.transZ = calculateTranslation(currentReads, 0, 2, 4, 6);
        motion.rotX = calculateRotation(currentReads, 1, 5);
        motion.rotY = calculateRotation(currentReads, 0, 4);
        motion.rotZ = calculateRotation(currentReads, 1, 3, 5, 7);

        return motion;
    }

    void updateLastActivityTime() {
        lastActivityTime = millis();
    }

private:
    array<Joystick, 4>& joysticks;
    int deadzone;
    unsigned long lastActivityTime;

    int16_t calculateTranslation(const std::array<int16_t, 8>& reads, int index1, int index2) {
        return (-reads[index1] + reads[index2]) / 2;
    }

    int16_t calculateTranslation(const std::array<int16_t, 8>& reads, int index1, int index2, int index3, int index4) {
        return (-reads[index1] - reads[index2] - reads[index3] - reads[index4]) / 2;
    }

    int16_t calculateRotation(const std::array<int16_t, 8>& reads, int index1, int index2) {
        return (-reads[index1] + reads[index2]) / 2;
    }

    int16_t calculateRotation(const std::array<int16_t, 8>& reads, int index1, int index2, int index3, int index4) {
        return (reads[index1] + reads[index2] + reads[index3] + reads[index4]) / 4;
    }
};

array<Joystick, 4> joysticks = {
    Joystick(16, 18),
    Joystick(3, 5),
    Joystick(11, 12),
    Joystick(7, 9)
};

static const uint8_t _hidReportDescriptor[] PROGMEM = {
    0x05, 0x01,           //  Usage Page (Generic Desktop)
    0x09, 0x08,           //  0x08: Usage (Multi-Axis)
    0xa1, 0x01,           //  Collection (Application)
    0xa1, 0x00,           // Collection (Physical)
    0x85, 0x01,           //  Report ID
    0x16, 0x00, 0x80,     //logical minimum (-500)
    0x26, 0xff, 0x7f,     //logical maximum (500)
    0x36, 0x00, 0x80,     //Physical Minimum (-32768)
    0x46, 0xff, 0x7f,     //Physical Maximum (32767)
    0x09, 0x30,           //    Usage (X)
    0x09, 0x31,           //    Usage (Y)
    0x09, 0x32,           //    Usage (Z)
    0x75, 0x10,           //    Report Size (16)
    0x95, 0x03,           //    Report Count (3)
    0x81, 0x02,           //    Input (variable,absolute)
    0xC0,                 //  End Collection

    0xa1, 0x00,           // Collection (Physical)
    0x85, 0x02,           //  Report ID
    0x16, 0x00, 0x80,     //logical minimum (-500)
    0x26, 0xff, 0x7f,     //logical maximum (500)
    0x36, 0x00, 0x80,     //Physical Minimum (-32768)
    0x46, 0xff, 0x7f,     //Physical Maximum (32767)
    0x09, 0x33,           //    Usage (RX)
    0x09, 0x34,           //    Usage (RY)
    0x09, 0x35,           //    Usage (RZ)
    0x75, 0x10,           //    Report Size (16)
    0x95, 0x03,           //    Report Count (3)
    0x81, 0x02,           //    Input (variable,absolute)
    0xC0,                 //  End Collection
    0xC0
};

Adafruit_USBD_HID usb_hid;

JoystickArray joystickArray(joysticks);
int16_t speed = 40;
Motion lastMotion;

void setup() {
    Serial.begin(115200);
    delay(7000);

    USB.PID(0xc631);
    USB.VID(0x256f);
    USB.begin();
    TinyUSBDevice.setManufacturerDescriptor("3DCONNEXION");
    TinyUSBDevice.setProductDescriptor("SpaceMouse Pro Wireless");
    TinyUSBDevice.setID(0x256f, 0xc631);
    usb_hid.setPollInterval(2);
    usb_hid.setReportDescriptor(_hidReportDescriptor, sizeof(_hidReportDescriptor));
    usb_hid.begin();

    while (!TinyUSBDevice.mounted()) delay(1);

    for (auto& joystick : joysticks) {
        joystick.readData();
        delay(100);  // Ensure stable readings
        joystick.setCenter();
    }
    joystickArray.calibrateDeadzone(); // Calibrate deadzone after centering
}

void sendCommand(const Motion& motion) {
    uint8_t trans[6] = {motion.transX & 0xFF, motion.transX >> 8, motion.transY & 0xFF, motion.transY >> 8, motion.transZ & 0xFF, motion.transZ >> 8};
    usb_hid.sendReport(1, trans, 6);
    uint8_t rot[6] = {motion.rotX & 0xFF, motion.rotX >> 8, motion.rotY & 0xFF, motion.rotY >> 8, motion.rotZ & 0xFF, motion.rotZ >> 8};
    usb_hid.sendReport(2, rot, 6);
}

bool motionChanged(const Motion& newMotion, const Motion& lastMotion) {
    return newMotion.transX != lastMotion.transX || newMotion.transY != lastMotion.transY || newMotion.transZ != lastMotion.transZ ||
           newMotion.rotX != lastMotion.rotX || newMotion.rotY != lastMotion.rotY || newMotion.rotZ != lastMotion.rotZ;
}

void loop() {
    if (!usb_hid.ready()) {
        Serial.println("USB not ready!");
    delay(20000);
        return;
    }
    Motion motion = joystickArray.calculateMotion();
    motion.applySpeed(speed);
    motion.log();

    if (motionChanged(motion, lastMotion)) {
        sendCommand(motion);
        lastMotion = motion;
    }

    delay(20000);
}