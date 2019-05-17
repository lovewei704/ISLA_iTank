/*
	iTank_AutoParking
	iTank Arduino 競賽範例：循軌+自動路邊停車

	此範例利用 Arduino Wire (I2C) 函式庫,
	利用　iTank 車頭底部的紅外線循軌感測器偵測軌道, 並循軌道行進
	(黑軌白軌可由iTank內建的設定功能表設定)
  循軌至停車點後, 利用後側紅外線測距器協助自動停車至停車位中

	執行前請先參考iTank使用手冊, 妥善設定紅外線循軌感測器
	以免iTank無法正常感測到軌道, 造成程式無法正常運作

	http://flagsupport.blogspot.tw
*/

// 引用 I2C 通訊函式庫
#include <Wire.h>

// 引用 FlagTank 通訊函式庫
#include <FlagTank.h>

// --------- 表示目前狀態的常數, 用於下方的 state 變數
#define STOP 0                // 停止狀態
#define GO   1                // 循軌狀態

#define PARKING_START 2       // 開始停車
#define PARKING_BACK 3        // 倒車斜插入停車格
#define PARKING_TURN 4        // 倒車進停車格後迴正
#define PARKING_ADJUST 5      // 迴正後倒車至後牆前側
#define PARKING_TURN_TIME 550 // 循軌至停止線後順時鐘轉向的時間

#define PARKING_BACK_DIS 95   // 倒車斜插入停車格的安全距離
#define PARKING_ADJ_DIS 100  // 與後牆安全距離

//#define DEBUG               // 用序列埠從 ZigBee 輸出感測數據
// 需搭配 FLAG-N002 ZigBee 無線網路模組

//----------------1234567890123
char help[][15] = {"Press",
                   "  K3 to Start",
                   "  K0-2 to Stop"
                  };

byte state;    // 目前iTank運作狀態,
// 程式中用此狀態控制iTank行為

char buf[14] = {0};
byte key = 0;               // 儲存讀取的按鍵值
byte line = 0, prevLine = 0; // 儲存循軌值
byte bumper;                // 儲存iTank前端碰撞感測器值
unsigned long startTime = 0; // 記錄計時動作的啟始時間
boolean PRE = true;

void setup() {
  Serial.begin(19200);

  iTank.begin();
  iTank.clearLCD(); // 清除畫面

  // 在 LCD 顯示訊息
  // 參數為輸出行號(0~5) 及要輸出的字串
  iTank.writeLCD(1, help[0]);
  iTank.writeLCD(2, help[1]);
  iTank.writeLCD(3, help[2]);

  state = STOP;
}

//用來顯示的符號
byte track = 92; // 黑色色塊
byte ground = 95; // 底線字元 '_'

void loop() {
  // 讀取紅外線循軌感測值
  // 傳回值的bit0~3分別代表左、中、右感測器的感測值
  // bit值為0表示無軌道, 1表示有軌道
  if (PRE)
  {
    prevLine = line;          // 記錄前次循軌值
  }
  line = iTank.readFloorIR(); // 讀取循軌值
  iTank.readDistanceIR();   // 讀取測距值

  // 顯示四個角落的測距結果
  // 左前=>顯示在LCD左上角
  // 測距結果240表示未偵測到物體
  // 為避免數值由3位數變成2位數時(例如 100=>99),
  // 原個位數數字仍留在LCD畫面, 故用 sprintf()
  // 及格式化字串 "%3d" 設定一律顯示3個字元寬
  sprintf(buf, "%3d", iTank.irDistance[0]);
  iTank.writeLCD(0, buf);

  // 右前=>顯示在LCD右上角
  // 240表示未偵測到物體
  iTank.writeLCDInt(0, iTank.irDistance[1]);

  // 左後=>顯示在LCD左下角
  // 240表示未偵測到物體
  sprintf(buf, "%3d", iTank.irDistance[2]);
  iTank.writeLCD(5, buf);

  // 右後=>顯示在LCD右下上角
  // 240表示未偵測到物體
  iTank.writeLCDInt(5, iTank.irDistance[3]);

  // 顯示循軌感測值於LCD第2列
  // 無軌道用 '_' 字元代表 (字碼95)
  // 有軌道則顯示黑色色塊  (字碼92)
  sprintf(buf, "    L:%c%c%c:R",
          (((line & 0x01) == 0x01) ? track : ground), // 左感測器
          (((line & 0x02) == 0x02) ? track : ground), // 中感測器
          (((line & 0x04) == 0x04) ? track : ground)); // 右感測器
  iTank.writeLCD(4, buf);

  // 取得K0~K3狀態
  byte key = iTank.readKey();

  // 依目前狀態決定處理方式
  // STOP: 停止中
  //       - 按K3開始循軌行進
  // GO: 循軌中
  //     - 碰撞到物體、使用者按K0~K2、
  //       3個感測器都偵測到軌道即準備停車
  //     - 非上述狀況維持循軌行進
  switch (state) {
    case STOP:    // 停止中
      // 按 K3 即依開始循軌行進
      if (key == 8) {
        PRE = true;
        state = GO;            // 變更模式
        iTank.writeMotor(2, 2); // 開始前進
        startTime = millis();
        break;
      }  // end of if(key==8)
      break;
    case GO:  // 循軌中
      bumper = iTank.readTouch();
      // 碰撞到物體或使用者按K0~K2 或
      // 3個感測器都偵測到軌道即停車
      if (bumper > 0 || ((key > 0) && (key != 8))) {
        state = STOP;
        iTank.stop(); // 停車
      }
      else if (line == 7 || (millis() - startTime)  == 4500) {
        PRE = false;
#ifdef DEBUG
        Serial.print("===Start:");
        Serial.println(prevLine);
#endif
        iTank.writeLCD(1, String((millis() - startTime), DEC));
        iTank.writeLCD(2, prevLine);

        iTank.writeMotor(-3, -3); // 稍往前衝避免慣性向後撞牆
        delay(10);
        state = PARKING_START;  // 進入停車狀態

        switch (prevLine)
        {
          case 1:
            iTank.writeMotor(1, -1);
            delay(25);
            break;
          case 3:
            iTank.writeMotor(-1, 1);
            delay(40);
            break;
          case 4:
            iTank.writeMotor(1, -1);
            delay(90);
            break;
          case 6:
            iTank.writeMotor(1, -1);
            delay(40);
            break;
          default:
            delay(50);
            break;
        }

        /*if (prevLine == 4) {
          iTank.writeMotor(1, -1);  // 如果車體偏軌道右側, 增加旋轉時間
          delay(95);
          }*/

        iTank.writeMotor(1, 1); // 往前移一小段以便讓出斜插入停車格的空間
        delay(350);

        iTank.writeMotor(2, -2); // 原地右轉, 準備停車
        /* if(prevLine == 4) { delay(250); }  // 如果車體偏軌道右側, 增加旋轉時間
          else if(prevLine == 6){ delay(100); }
          else if(prevLine == 3){ delay(100); }
          else if(prevLine == 2){ delay(100); }
          else if(prevLine == 1){ delay(100); }
          iTank.stop();*/

        startTime = millis();   // 記錄開始原地右轉的時間時間
      }
      else {
        // 呼叫循軌函式
        tryFollowLine();
      }
      break;
    case PARKING_START:
      printIr();
      if (millis() - startTime > PARKING_TURN_TIME * 1.1) { // 已經轉夠時間
        #ifdef DEBUG
        Serial.print("===back:");
        Serial.println(line);
        #endif
        state = PARKING_BACK;     // 進入後退階段
        iTank.writeMotor(-1, -1); // 後退斜進入停車格
      }
      break;
    case PARKING_BACK:
      printIr();
      if (iTank.irDistance[2] < PARKING_BACK_DIS) { // 已經退到靠左牆近處
        iTank.writeMotor(3, 3); // 稍往前衝避免慣性向後撞牆
        delay(25);
        iTank.writeMotor(-7, 7); // 迴正
        startTime = millis();   // 記錄開始迴正的時間
        #ifdef DEBUG
        Serial.println("===TURN");
        #endif
        state = PARKING_TURN;
      }
      break;
    case PARKING_TURN:
      printIr();

      if ((millis() - startTime) > (PARKING_TURN_TIME * 1.5))/* || // 已迴正過久
         (iTank.irDistance[3] == 240))*/ {    // 已迴正到車體右側斜出後牆右邊緣
        iTank.writeMotor(-1, -1);   // 後退至後牆面前
        #ifdef DEBUG
        Serial.println("===ADJ");
        #endif
        state = PARKING_ADJUST;
      }
      else { // 以左轉、後退方式讓車體進入停車格內慢慢迴正
        iTank.writeMotor(-4, 4);
        delay(16);
        iTank.writeMotor(-1, -1);
        delay(8);
      }
      break;
    case PARKING_ADJUST:
      printIr();
      if (iTank.irDistance[3] < PARKING_ADJ_DIS ||  // 左後已靠近後牆面
          iTank.irDistance[2] < PARKING_ADJ_DIS) {  // 右後已靠近後牆面
        iTank.writeMotor(2, 2);
        delay(20);
        state = STOP;
        iTank.stop();
      }

  } // end of switch(state)
}

// 循軌函式
// 以簡單的邏輯進行控制: 軌道在左即左轉
//                       軌道在右即右轉
//                       軌道在中即直行
void tryFollowLine(void)
{
  switch (line) {
    case 3: // 左邊及中間感測到軌道
      iTank.writeMotor(-1, 1); // 左轉前進
      break;
    case 1: // 左邊感測到軌道
      iTank.writeMotor(-1, 2); // 左轉前進
      break;
    case 2: // 中間感測到軌道
      iTank.writeMotor(5, 5); // 直行
      break;
    case 4: // 右邊感測到軌道
      iTank.writeMotor(2, -1); // 右轉前進
      break;
    case 6: // 右邊及中間感測到軌道
      iTank.writeMotor(1, -1); // 右轉前進
      break;
      // default: 若感測到沒有軌道 (感測值為0),
      // 則仍維持前一次的方式行進
  }
  delay(50);
}

void printIr() {
#ifdef DEBUG
  sprintf(buf, "%3d,%3d\n", iTank.irDistance[2], iTank.irDistance[3]);
  Serial.println(buf);
#endif
}

