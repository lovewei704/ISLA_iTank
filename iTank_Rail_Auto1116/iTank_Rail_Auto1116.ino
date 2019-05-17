/*
	iTank 尋軌避障範例
 
	此範例利用 Arduino Wire (I2C) 函式庫,
	利用　iTank 車頭底部的紅外線循軌感測器偵測軌道, 並循軌道行進
	(黑軌白軌可由iTank內建的設定功能表設定) 
  行進時會利用車頭的追物模組偵測路徑上的障礙物, 並以左轉繞 [ 形狀路徑避過障礙
	
	執行前請先參考iTank使用手冊, 妥善設定紅外線循軌感測器與追物模組
	以免iTank無法正常感測到軌道與障礙物, 造成程式無法正常運作
	
	http://flagsupport.blogspot.tw
 */
 
// 引用 I2C 通訊函式庫
#include <Wire.h> 

// 引用 FlagTank 通訊函式庫
#include <FlagTank.h> 

// --------- 表示目前狀態的常數, 用於下方的 state 變數
#define STOP 0          // 停止狀態
#define GO   1          // 尋軌狀態
#define L_TURN_1 2      // 左轉避障第一階段
#define L_TURN_2 3      // 左轉避障第二階段
#define R_TURN 4        // 右轉完回軌道階段

//----------------1234567890123
char help[][15]={"Press",
								 "  K3 to Start",  
								 "  K0-2 to Stop"};

byte state;    // 目前iTank運作狀態,
							 // 程式中用此狀態控制iTank行為

char buf[14]={0};           // 格式化字串用
byte key = 0;               // 儲存讀取的按鍵值
byte line = 0;              // 儲存循軌值
byte bumper;                // 儲存iTank前端碰撞感測器值
byte tryAvoidance = 0;      // 避障次數
unsigned long startTurn;    // 開始避障左轉的時間
unsigned long cleared;      // 完全躲開障礙的時間
byte dip = 3;               // 空瓶位置
byte track =92;             // 代表軌道的黑色色塊 
byte ground=95;             // 代表地面的底線字元 '_'
boolean slowMode = false;
boolean mustLeftTurn = false;
unsigned long left;
Chaser chs;                 // 追物模組感測值

void setup() {
	Serial.begin(19200);
	
	iTank.begin();    // 啟始程式庫
	iTank.clearLCD(); // 清除畫面
	
	// 在 LCD 顯示訊息
	// 參數為輸出行號(0~5) 及要輸出的字串
	iTank.writeLCD(0, help[0]);
	iTank.writeLCD(1, help[1]);  
	iTank.writeLCD(2, help[2]);

	state = STOP;  
}

void loop() {
  line=iTank.readFloorIR(); // 讀取紅外線循軌感測值
  chs=iTank.readChaserIR(); // 讀取追物模組感測值
  dip = iTank.readDip();    // 讀取指撥開關狀態值
  
  // 顯示循軌感測值於LCD第2列
  // 無軌道用 '_' 字元代表 (字碼95) 
  // 有軌道則顯示黑色色塊  (字碼92)
  sprintf(buf,"    L:%c%c%c:R",
    (((line&0x01)==0x01)?track:ground),  // 左感測器
    (((line&0x02)==0x02)?track:ground),  // 中感測器
    (((line&0x04)==0x04)?track:ground)); // 右感測器 
  iTank.writeLCD(4, buf);

  // 取得K0~K3狀態
  byte key = iTank.readKey(); 
	
  // 依目前狀態決定處理方式
  // STOP: 停止中 
  //       - 按K3開始循軌行進
  // GO: 循軌中
  //     - 碰撞到物體、使用者按K0~K2、
  //       3個感測器都偵測到軌道即停車
  //     - 非上述狀況維持循軌行進
  switch(state){
    case STOP:                  // 停止中
      if(key==8){               // 按 K3 即依開始循軌行進 
        state=GO;               // 變更模式      
        iTank.writeMotor(3,3);  // 開始前進
        tryAvoidance = 0;       // 將避障次數歸零
        break;
      }
      break;
    case GO:  // 循軌中
      bumper=iTank.readTouch();
      // 碰撞到物體或使用者按K0~K2 或
      // 3個感測器都偵測到軌道即停車
      if(bumper>0 ||              // 發生碰撞
         ((key>0)&&(key!=8)) ||   // 按 K0~K2
         // 碰到終止線且已經避過 3 個障礙而且避障完超過 3 秒以上
         (line==7 && tryAvoidance >=3 && (millis() - cleared > 3000))) {
        state=STOP;   // 回到停止狀態
        iTank.stop(); // 停車
        delay(100);
      }
      else {
        // 呼叫循軌函式
        tryFollowLine();
      }
      break;
    case L_TURN_1:              // ① 左轉避障第一階段
      if(line==0) {             // 原地左轉後從完全沒偵測到軌道起算計時
        startTurn = millis();   // 記錄開始時間
        state = L_TURN_2;       // 進入左轉避障第二階段
      }
      break;
    case L_TURN_2:
      if((millis() - startTurn) > 400) { // 已左轉足夠時間
        state = R_TURN;         // 進入避障右轉階段
        iTank.writeMotor(3,3);  // ② 先直行
        delay(570);
        iTank.writeMotor(3,-3); // ③ 原地右轉
        if(dip != 4 && tryAvoidance == 2) { // 如果是 4 號位置的障礙
          delay(850);           // 4 號位置幅度大, 轉的時間較久
        }
        else {
          delay(715);
        }
        iTank.writeMotor(3,3);  // ④ 右轉完直行
        if(dip != 4 && tryAvoidance == 2) { // 若是 4 號位置的障礙
          delay(750);
        }
        else if ((dip < 3 && tryAvoidance == 1) || (dip == 4 && tryAvoidance == 2)) {
           // 若是 3 號位置的障礙
           mustLeftTurn = true;
          delay(450);
        }else {  // 若是 1、2 號位置的障礙
          delay(650);
        }
        iTank.writeMotor(3,-3); // ⑤ 直行後右轉準備回軌道上
        if(dip != 4 && tryAvoidance == 2) { // 若是 4 號位置的障礙
          delay(1200);
        }
        else if ((dip < 3 && tryAvoidance == 1) || (dip == 4 && tryAvoidance == 2)) {
          // 若是 3 號位置的障礙
          iTank.writeMotor(-3,3);
          delay(150);
        }
        else {  // 若是 1、2 號位置的障礙
          if(dip = 3) delay(100);
          delay(550);
        }
        iTank.writeMotor(2,2); // ⑥ 右轉完後直行回軌道
      } else if(line!=0) {  // 左轉過程中仍有偵測到軌道就重新計時
        state = L_TURN_1;   // 回到左轉避障第一階段
      }
      break;
    case R_TURN:            // 右轉完回軌道階段
      if(line!=0) {         // 偵測到軌道
        tryAvoidance++;     // 增加避過的障礙計數
        if(tryAvoidance == 3){  // 已經避過 3 個障礙
          cleared == millis();  // 記錄避過第 3 個障礙的時間
        }
        if(dip < 3 && tryAvoidance == 2)
        {
          slowMode = true;
        }
        if(mustLeftTurn){ left = millis(); }
        state=GO;               // 回覆到尋軌狀態
        iTank.writeMotor(3,3);  // 直行
      }
      break;
    } // end of switch(state)
}

// 左轉再繞右弧線避開障礙
void turnAndRun()
{
  iTank.writeLCD(5,tryAvoidance);     // 顯示避障次數
  state = L_TURN_1;                   // 進入 ① 左轉避障第一階段
  if(dip != 4 && tryAvoidance == 2) { // 如果要避開的是 4 號位置的空瓶
    iTank.writeMotor(-2,-2);          // 先後退 100ms
    delay(100);
  }
  iTank.writeMotor(-3,3);             // 原地左轉避開障礙
}

// 循軌函式
// 以簡單的邏輯進行控制: 軌道在左即左轉
//                       軌道在右即右轉
//                       軌道在中即直行
void tryFollowLine(void)
{
  if(millis() - left > 50) mustLeftTurn = false;
  if(!chs.noObject || chs.inside) { // 如果偵測到障礙物
    if(tryAvoidance == 2)
    { slowMode = false; }
    
    turnAndRun();                   // 進入避障程序
  } else {
    switch(line){
      case 3: // 左邊及中間感測到軌道
        iTank.writeMotor(-1,2); // 左轉前進
        if(slowMode) iTank.writeMotor(-1,1);
      	break;
            case 1: // 左邊感測到軌道
              iTank.writeMotor(-1,3); // 左轉前進
              if(slowMode) iTank.writeMotor(-1,2);
      	break;  
            case 2: // 中間感測到軌道
              iTank.writeMotor(3,3); // 直行
              if(slowMode) iTank.writeMotor(2,2);
              if(mustLeftTurn)iTank.writeMotor(-1,3);
      	break;
            case 4: // 右邊感測到軌道
      	      iTank.writeMotor(3,-1); // 右轉前進
              if(slowMode) iTank.writeMotor(2,-1);
              if(mustLeftTurn)(-1,3);
      	break;  
            case 6: // 右邊及中間感測到軌道
              iTank.writeMotor(2,-1); // 右轉前進
              if(slowMode) iTank.writeMotor(1,-1);
              if(mustLeftTurn)iTank.writeMotor(-1,2);
      	break;
      	// default: 若感測到沒有軌道 (感測值為0),
      	// 則仍維持前一次的方式行進            
    }
    delay(50);
  }
}
