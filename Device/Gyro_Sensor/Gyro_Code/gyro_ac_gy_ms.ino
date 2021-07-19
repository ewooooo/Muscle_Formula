/*
 * GY-86 
 *
 * GY-86은 MPU6050 가속도 자이로 센서, HMC5883L 지자기 센서 그리고
 * MS561101BA 기압 센서를 조그만 보드에 모두 갖춘 10 DOF(Degrees Of
 * Freedom)를 지원하는 기기입니다.
 *
 * 이 스케치는 
 *
 * - MPU6050으로부터 가공하지 않은 가속도, 자이로 센서의 16비트
 *   데이터들인 ax, ay, az, gx, gy, gz 등을 읽어내고,
 * 
 * - 지자기 센서 데이터는 MPU6050을 통하여 HMC5883L로부터 16비트
 *   데이터 mx, my, mz를 읽어 냅니다
 *
 * - 기압 센서 MS561101BA를 통하여 온도, 기압 그리고 해면 높이를
 *   이용하여 높이를 측정합니다.
 * 
 */
 
/*
 * I2Cdev와 MPU6050 라이브러리들이 설치되 있어야 하며, I2Cdev가 
 * 아두이노의 Wire 기반으로 개발되어 있다면, Wire 라이브러리가
 * 필요합니다.
 */
#include "I2Cdev.h"
#include "MPU6050.h"
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
#include <Wire.h>
#endif
 
/*
 * MS561101BA 기압계를 위한 라이브러리가 설치되 있어야 합니다.
 */
#include <MS561101BA.h>
 
#define HMC5883L_DEFAULT_ADDRESS    0x1E
#define HMC5883L_RA_DATAX_H         0x03
#define HMC5883L_RA_DATAZ_H         0x05
#define HMC5883L_RA_DATAY_H         0x07
 
// MPU6050 I2C 주소는 기본적으로 0x68이며, AD0를 HIGH로 놓으면
// 주소를 0x69로 사용할 수 있습니다. 
MPU6050 mpu;
// MPU6050 mpu(0x69); // <-- AD0가 HIGH로 주소를 0x69를 사용할 경우
 
int16_t ax, ay, az;
int16_t gx, gy, gz;
int16_t mx, my, mz;
 
float heading;
 
/* Note:
 * ====
 * 정확한 높이를 측정하기 위해서는 해면 기압 정보를 사용하여야 합니다.
 * http://www.kma.go.kr/weather/observation/aws_table_popup.jsp 로부터
 * 기압 정보를 얻어 sea_press에 넣었습니다.
 */
MS561101BA baro = MS561101BA();
 
#define MOVAVG_SIZE             32
#define STANDARD_SEA_PRESSURE  1013.25
 
const float sea_press = 1014.0;
float altitude, temperature, pressure;
float movavg_buff[MOVAVG_SIZE];
int movavg_i=0;
 
void setup() {
  // I2C 버스를 사용하기 위하여 Wire.begin()를 실행합니다. (I2Cdev
  // 라이브러리에서 이를 자동으로 실행치 않기 때문입니다)
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
  Wire.begin();
  TWBR = 12; // 400kHz I2C clock (200kHz if CPU is 8MHz)
 
#elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
  Fastwire::setup(400, true);
#endif
 
  // 시리얼 통신을 115200bps로 초기화합니다
  Serial.begin(115200);
  
  // I2C 버스에 연결되어 있는 기기들을 초기화합니다
  Serial.println(F("I2C 기기들 초기화 중..."));
  mpu.initialize();
 
  // 연결을 검증합니다.
  Serial.println(F("기기 연결 시험 중..."));
  Serial.print(F("MPU6050 연결 "));
  Serial.println(mpu.testConnection() ? F("정상") : F("오류"));
 
  /*
   * 지자기 센서 HMC5883L은 MPU6050의 보조 I2C 버스에 연결되어 있어,
   * MPU6050을 통하여 지자기 센서를 읽어 낼 수 있습니다. 이를 위한
   * 환경 구성을 진행합니다.
   *
   * 1. MPU6050의 보조 I2C 버스를 직접 R/W하기 위하여 마스터 모드를
   *    중지하고, 바이패스 모드를 활성화합니다.
   *
   * 2. HMC5883L 모드를 연속 측정 모드로 측정 간격을 75Hz로 설정합니다
   *
   * 3. 바이패스 모드를 비활성화(중지)합니다
   *
   * 4. MPU6050에게 mx, my, mz를 읽어들이기 위한 HMC5883L 레지스터 주소
   *    그리고 읽어들인 데이터의 MSB와 LSB를 바꿔야 하는지, 데이터 크기
   *    등을 설정합니다.
   *
   * 5. 최종적으로 HMC5883L로부터 지자기 센서 데이터는 MPU6050을 통하여
   *    읽어들인다고 마스터 모드를 활성화합니다. 
   * 
   */ 
  mpu.setI2CMasterModeEnabled(0);
  mpu.setI2CBypassEnabled(1);
 
  Wire.beginTransmission(HMC5883L_DEFAULT_ADDRESS);
  Wire.write(0x02); 
  Wire.write(0x00);  // 연속 읽기 모드로 설정
  Wire.endTransmission();
  delay(5);
 
  Wire.beginTransmission(HMC5883L_DEFAULT_ADDRESS);
  Wire.write(0x00);
  Wire.write(B00011000);  // 75Hz
  Wire.endTransmission();
  delay(5);
 
  mpu.setI2CBypassEnabled(0);
 
  // X 축 WORD 
  /* 0x80은 7번째 비트를 1로 만듭니다. MPU6050 데이터 자료에 읽기 모드
   * 설정은 '1'로 쓰기 모드 설정은 '0'로 하여야 한다고 나와 있습니다. */
  mpu.setSlaveAddress(0, HMC5883L_DEFAULT_ADDRESS | 0x80);
  mpu.setSlaveRegister(0, HMC5883L_RA_DATAX_H);
  mpu.setSlaveEnabled(0, true);
  mpu.setSlaveWordByteSwap(0, false);
  mpu.setSlaveWriteMode(0, false);
  mpu.setSlaveWordGroupOffset(0, false);
  mpu.setSlaveDataLength(0, 2);
 
  // Y 축 WORD
  mpu.setSlaveAddress(1, HMC5883L_DEFAULT_ADDRESS | 0x80);
  mpu.setSlaveRegister(1, HMC5883L_RA_DATAY_H);
  mpu.setSlaveEnabled(1, true);
  mpu.setSlaveWordByteSwap(1, false);
  mpu.setSlaveWriteMode(1, false);
  mpu.setSlaveWordGroupOffset(1, false);
  mpu.setSlaveDataLength(1, 2);
 
  // Z 축 WORD
  mpu.setSlaveAddress(2, HMC5883L_DEFAULT_ADDRESS | 0x80);
  mpu.setSlaveRegister(2, HMC5883L_RA_DATAZ_H);
  mpu.setSlaveEnabled(2, true);
  mpu.setSlaveWordByteSwap(2, false);
  mpu.setSlaveWriteMode(2, false);
  mpu.setSlaveWordGroupOffset(2, false);
  mpu.setSlaveDataLength(2, 2);
 
  // HMC5883L은 MPU6050을 통하여 읽을 수 있어, 마스터 모드를 ON합니다
  mpu.setI2CMasterModeEnabled(1);
 


    
  // 시리얼 모니터로부터 시작 데이터를 기다립니다
  Serial.println(F("\n준비되었으면 데이터를 보내세요: "));
  while (Serial.available() && Serial.read()); // 버퍼를 비웁니다
  while (!Serial.available());                 // 데이터를 기다립니다
  while (Serial.available() && Serial.read()); // 버퍼를 다시 지웁니다
 
}
 
void loop() {
  // MPU6050으로부터 가속도 자이로 센서의 가공하지 않은 데이터를 읽어
  // 들입니다
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  
  // MPU6050을 통하여 HMC5883L 지자기 센서 데이터 값을 읽어 들입니다
  mx=mpu.getExternalSensorWord(0);
  my=mpu.getExternalSensorWord(2);
  mz=mpu.getExternalSensorWord(4);
 
  // 어디를 향하고 있는 heading 값을 구합니다. 만약 - 값이면 이를 +
  // 값으로 바꿉니다 
  heading = atan2(my, mx);
  if(heading < 0) heading += 2 * M_PI;

  // 가속도/자이로/지자기 x/y/z 값들을 시리얼 모니터로 출력합니다
  Serial.println(F("a/g/m:\tax\tay\taz\tgx\tgy\tgz\tmx\tmy\tmz"));
  Serial.print(F("a/g/m:\t"));
  Serial.print(ax); Serial.print("\t");
  Serial.print(ay); Serial.print("\t");
  Serial.print(az); Serial.print("\t");
  Serial.print(gx); Serial.print("\t");
  Serial.print(gy); Serial.print("\t");
  Serial.print(gz); Serial.print("\t");
  Serial.print(mx); Serial.print("\t");
  Serial.print(my); Serial.print("\t");
  Serial.print(mz); Serial.print("\n");
 
  // Processing Compass 스케치와 동작시키기 위한 heading 데이터를
  // 출력합니다.
  Serial.print('@');
  Serial.println(heading, 6);
  
  // 온도와 기압 그리고 높이를 출력합니다
//  Serial.print(temperature);
//  Serial.print(F(" ℃ 기압: "));
//  Serial.print(pressure);
//  Serial.print(F(" mbar 고도: "));
//  Serial.print(altitude);
//  Serial.println(F("m"));
  
}
 
 
void pushAvg(float val) {
  movavg_buff[movavg_i] = val;
  movavg_i = (movavg_i + 1) % MOVAVG_SIZE;
}
 
float getAvg(float * buff, int size) {
  float sum = 0.0;
  for(int i=0; i<size; i++) {
    sum += buff[i];
  }
  return sum / size;
}
 
