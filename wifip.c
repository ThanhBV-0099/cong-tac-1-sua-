#include "wifip.h"
#ifdef USE_WIFI 
#include "wifi.h"
#include "mcu_api.h" 
#else
#include "zigbee.h"
#include "mcu_api.h"

#endif

	#define TOUCH1_PAD1 HAL_GPIO_ReadPin(TOUCH_PAD1_GPIO_Port,TOUCH_PAD1_Pin)
	#define LED1_RED SW1_LED_RED_GPIO_Port,SW1_LED_RED_Pin
	#define LED1_BLUE SW1_LED_BLUE_GPIO_Port,SW1_LED_BLUE_Pin

////////////////struct rf
typedef struct
{
	uint8_t SOF;
	uint8_t STATE_SW1;
	uint8_t STATE_SW2;
	uint8_t STATE_SW3;
	uint8_t STATE_SW4;
	uint64_t MAC;
	uint8_t EOF1;
	uint8_t EOF2;
}FRAME_RECEIVE_RF;
volatile FRAME_RECEIVE_RF Frame_Receive_Rf;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart1;
volatile uint8_t Nhanbuff,state_receive = 0;
volatile uint8_t Nhanbuff_rf[200],Nhan_rf,state_receive_rf = 0,count_rf = 0;
void Uart_PutChar(unsigned char value)
{
	HAL_UART_AbortReceive_IT(&huart1);
	HAL_UART_Transmit(&huart1,&value,1,100);
	HAL_UART_Receive_IT(&huart1,&Nhanbuff,1);
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance==huart1.Instance)//nhan du lieu uart1
	{
		#ifdef USE_WIFI 
		uart_receive_input(Nhanbuff);
		#else
		uart_receive_input(Nhanbuff);
		#endif
		//tiep tuc nhan du lieu
		HAL_UART_Receive_IT(&huart1,&Nhanbuff,1);
	}
	#ifdef USE_WIFI 
	else if(huart->Instance==huart2.Instance)//nhan du lieu uart2
	{
		Nhanbuff_rf[count_rf] = Nhan_rf;
		if(Nhanbuff_rf[count_rf-1] == '@' && Nhanbuff_rf[count_rf]=='c')
    {
      count_rf=0;
      state_receive_rf=1;
			Frame_Receive_Rf.SOF = Nhanbuff_rf[0];
			Frame_Receive_Rf.STATE_SW1 = Nhanbuff_rf[1];
			Frame_Receive_Rf.STATE_SW2 = Nhanbuff_rf[2];
			Frame_Receive_Rf.STATE_SW3 = Nhanbuff_rf[3];
			Frame_Receive_Rf.STATE_SW4 = Nhanbuff_rf[4];
			Frame_Receive_Rf.MAC = 	(uint64_t)Nhanbuff_rf[5]<<56 | (uint64_t)Nhanbuff_rf[6]<<48 | 
															(uint64_t)Nhanbuff_rf[7]<<40 | (uint64_t)Nhanbuff_rf[8]<<32 | (uint64_t)Nhanbuff_rf[9]<<24 | 
															(uint64_t)Nhanbuff_rf[10]<<16 | (uint64_t)Nhanbuff_rf[11]<<8 | (uint64_t)Nhanbuff_rf[12];
			
			Frame_Receive_Rf.EOF1 = Nhanbuff_rf[13];
			Frame_Receive_Rf.EOF2 = Nhanbuff_rf[14];
			
    }
    else
    {
      state_receive_rf=0;
			
      count_rf++;
    }
		//tiep tuc nhan du lieu
		HAL_UART_Receive_IT(&huart2,&Nhan_rf,1);
	}
	#endif
}
unsigned char wifi_state;
volatile unsigned char State_switch_1;
volatile unsigned char State_switch_2;
volatile unsigned char State_switch_3;
volatile unsigned char State_switch_4;
unsigned long State_countdown_1;
unsigned long State_countdown_2;
unsigned long State_countdown_3;
unsigned long State_countdown_4;
unsigned long State_thong_so1; // thong so add vao cong suat toi thieu 10 wh
unsigned long State_thong_so2; // thong so dong dien don vi mA
unsigned long State_thong_so3; // Thong so cong suat don vi chia 10 ra W
unsigned long State_thong_so4; // thong so dien ap chia 10 ra vol
unsigned long State_thong_so1_count; // thong so add vao cong suat toi thieu 10 wh
unsigned long State_thong_so2_count; // thong so dong dien don vi mA
unsigned long State_thong_so3_count; // Thong so cong suat don vi chia 10 ra W
unsigned long State_thong_so4_count; // thong so dien ap chia 10 ra vol
float diennang = 0;
unsigned char dodienap = 0,nead_update_dienanng = 0;
uint16_t count_nhay = 0,count_update = 0,count_setup = 0,time_count_setup = 0,count_reset_heart = 0;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	#ifndef CONG_TAC_MOI
	if(GPIO_Pin == GPIO_PIN_10)
	{
		State_thong_so3_count ++;
	}
	else if(GPIO_Pin == GPIO_PIN_2)
	{
		if(dodienap == 0) // do dong dien
		{
			State_thong_so2_count++;
		}
		else
		{
			State_thong_so4_count++;
		}
	}
	#else
	if(GPIO_Pin == GPIO_PIN_2)
	{
		State_thong_so3_count ++;
	}
	else if(GPIO_Pin == GPIO_PIN_10)
	{
		if(dodienap == 0) // do dong dien
		{
			State_thong_so2_count++;
		}
		else
		{
			State_thong_so4_count++;
		}
	}
	#endif
}


//thong so 4 1605Hz tuong ung 220V
//thong so 3 20 tuong ung 25W
//thong so 2 12 tuong ung 0.11A

void process_diennang(void)
{
	static uint16_t count_do_dienap_dongdien = 0;
	static uint16_t count_do_dienanng = 0;
	
	if(nead_update_dienanng == 1)
	{
		if(count_do_dienanng >=5) // 5 s update 1 lan
		{
			count_do_dienanng = 0;
			nead_update_dienanng = 0;
		}
		else
		{
			count_do_dienanng++;
		}
	}
	else
	{
		count_do_dienanng = 0;
	}
	
	
	if(count_do_dienap_dongdien>=2)
	{
		count_do_dienap_dongdien = 0;
		if(dodienap == 0)
		{
			HAL_GPIO_WritePin(SEL_GPIO_Port,SEL_Pin,GPIO_PIN_SET); // bat de do dong dien
			dodienap = 1;
		}
		else
		{
			HAL_GPIO_WritePin(SEL_GPIO_Port,SEL_Pin,GPIO_PIN_RESET); // bat de do dong dien
			dodienap = 0;
		}
	}
	else
	{
		count_do_dienap_dongdien ++;
	}
	
	if(State_thong_so3_count>100)//cho nay de chia dai cong tru cho phu hop
	{
		State_thong_so3 = State_thong_so3_count*13;
	}
	else
	{
		State_thong_so3 = State_thong_so3_count*12;
	}
	
	State_thong_so3_count = 0;
	
	if(dodienap == 0) // do dong dien
	{
		State_thong_so2 = State_thong_so2_count*10;
		State_thong_so2_count = 0;
		
	}
	else// do dien ap
	{
		State_thong_so4 = (float)State_thong_so4_count*1.34;
		State_thong_so4_count = 0;
		
	}
	
	diennang += (float)State_thong_so3 / 36000.0;  //chia 3600 de 1 s + 1 lan, du 3600s la bang cong suat
	if(diennang>10 && nead_update_dienanng == 0)
	{
		State_thong_so1 = 10;
		diennang -= 10;
		nead_update_dienanng = 1;
		count_update = TIME_NEED_UPDATE;
	}
}

void coundown_process(void)
{
	static uint16_t count_1s = 0;
	
	if(count_1s >= 1000)
	{
		count_1s = 0;
		//cu 1 s do dien nang 1 lan
		process_diennang();
		
		//process cho count down 1
		if(State_countdown_1 > 0)
		{
			if(State_countdown_1 >1)
			{
				State_countdown_1 --;
			}
			else //neu dung bang 1 thi togle thiet bi
			{
				State_countdown_1 = 0;
				count_update = TIME_NEED_UPDATE;
				if(State_switch_1 == 0)
				{
					State_switch_1 = 1;
				}
				else
				{
					State_switch_1 = 0;
				}
			}
		}
		else
		{
			State_countdown_1 = 0;
		}
	}
	else
	{
		count_1s ++;
	}
	
}

void wifiprocess(void)
{
	static uint16_t count_wifi_status = 0,count_blink_1s = 0,modeconfig = 0,timeout_config = 0,count_wifi_status_blink = 0,
									old_pad1 = 0,old_pad2 = 0,old_pad3 = 0,old_pad4 = 0,count_config_wifi = 0,state_config = 0,old_state1 = 0,
									old_state2 = 0,old_state3 = 0,old_state4 = 0,timeout_update_rf = 0,count_reset_touch = 0,time_count_reset_touch = 0,flag_reset_touch = 0,
									cycle_count_reset_touch = 0;;
	static uint8_t has_change_touchpad = 0,old_button = 0;
	static uint8_t buff_send_rf[6]={'A','X','X','X','X','@'};
	#ifdef USE_WIFI 
	wifi_uart_service();
		wifi_state = mcu_get_wifi_work_state();
		if(wifi_state == WIFI_LOW_POWER)
		{
//			sprintf(m,"LOWPOW");
		}
		else if(wifi_state == SMART_CONFIG_STATE)
		{
			count_wifi_status_blink = 25;
//			sprintf(m,"CONFIG");
		}
		else if(wifi_state == AP_STATE)
		{
//			sprintf(m,"AP_STA");
		}
		else if(wifi_state == WIFI_NOT_CONNECTED)
		{
			count_wifi_status_blink = 200;
//			sprintf(m,"NOT_CO");
		}
		else if(wifi_state == WIFI_CONNECTED)
		{
			count_wifi_status_blink = 0;
//			sprintf(m,"CONNEC");
		}
		else if(wifi_state == WIFI_CONN_CLOUD)
		{
//			sprintf(m,"CLOUD ");
			count_wifi_status_blink = 0;
		}
		else if(wifi_state == WIFI_SATE_UNKNOW)
		{
			count_wifi_status_blink = 1;
//			sprintf(m,"UNKNOW");
		}
		#else
		zigbee_uart_service();
		#endif
		//count cho hien thi trang thai wifi
		if(count_wifi_status_blink == 0)
		{
			HAL_GPIO_WritePin(WIFI_STATUS_GPIO_Port,WIFI_STATUS_Pin,GPIO_PIN_SET);
		}
		else if(count_wifi_status> count_wifi_status_blink)
		{
			count_wifi_status = 0;
			HAL_GPIO_TogglePin(WIFI_STATUS_GPIO_Port,WIFI_STATUS_Pin);
		}
		else
		{
			count_wifi_status++;
		}
		
		
		
		/////count cho update data;
		if(count_update> TIME_UPDATE)
		{
			if(nead_update_dienanng == 1)
			{
				count_update = 400;
			}
			else
			{
				count_update = 0;
			}

			#ifdef USE_WIFI 
			all_data_update();
			#else
			all_data_update();
			#endif
			
			State_thong_so1 = 0;
			
		}
		else
		{
			count_update++;
		}
		HAL_UART_Receive_IT(&huart2,&Nhan_rf,1);
		HAL_UART_Receive_IT(&huart1,&Nhanbuff,1);
		
		//het timeout cua count setup
		
		if(time_count_setup> 100)
		{
			//time_count_setup = 0;
			//neu het timeout ma count chua lon hon 6 thi clear di
				count_setup = 0;
			old_button = 0;
			time_count_setup = 0;
		}
		else
		{
			time_count_setup++;
		}
		
		//count cho blink cac che do
		if(count_blink_1s> 50)
		{
			count_blink_1s = 0;
			if(modeconfig == 1)  // che do cho led nhay
			{
				//neu o cho do config thi nhay cac led len
				if(timeout_config >= 30)
				{
					modeconfig = 0;
					timeout_config = 0;
				}
				else
				{
					timeout_config++;
				}
				HAL_GPIO_WritePin(LED1_RED,GPIO_PIN_RESET);
				HAL_GPIO_TogglePin(LED1_BLUE);
			}
			else
			{
				modeconfig = 0;
				timeout_config = 0;
			}
		}
		else
		{
			count_blink_1s++;
		}
		
		//dinh ky reset touch 1 lan
		if(cycle_count_reset_touch >= PERIOD_TO_RESET_TOUCH)
		{
			cycle_count_reset_touch = 0;
			flag_reset_touch = 1;
		}
		else
		{
			cycle_count_reset_touch ++;
		}
		
		//cho can de reset touch
		if(flag_reset_touch == 1)
		{
			//Cho nay can reset touch
			
			HAL_GPIO_WritePin(VTOUCH_DK_GPIO_Port,VTOUCH_DK_Pin,GPIO_PIN_SET); // TAT touc
			if(time_count_reset_touch >= 50)
			{
				time_count_reset_touch = 0;
				flag_reset_touch = 2;
				//cho nay can bat touch
				HAL_GPIO_WritePin(VTOUCH_DK_GPIO_Port,VTOUCH_DK_Pin,GPIO_PIN_RESET); // BAT touc
			}
			else
			{
				time_count_reset_touch++;
			}
		}
		//cho can de reset touch
		else if(flag_reset_touch == 2)
		{
			HAL_GPIO_WritePin(VTOUCH_DK_GPIO_Port,VTOUCH_DK_Pin,GPIO_PIN_RESET); // BAT touc
			if(time_count_reset_touch >= 50)
			{
				time_count_reset_touch = 0;
				flag_reset_touch = 0;
				//cho nay can bat touch
			}
			else
			{
				time_count_reset_touch++;
			}
		}
		else
		{
			time_count_reset_touch = 0;
		}
		
		//nut 1
		if(TOUCH1_PAD1 ==GPIO_PIN_SET && time_count_reset_touch == 0 )
		{
			time_count_setup = 0;
			cycle_count_reset_touch = 0;
			if(count_config_wifi >= 200 && count_setup == NUM_OFF_COUNT_SETUP )
			{
				count_config_wifi = 200;
				#ifdef USE_WIFI 
				mcu_set_wifi_mode(0);
				#else
				mcu_network_start();
				#endif
				modeconfig = 1;//cho bang 1 de nhay led the hien dang che do config
				count_setup = 0;
			}	
			else
				count_config_wifi ++;
			
			if(count_reset_touch >= TIME_NEED_TO_RESET_TOUCH && flag_reset_touch == 0 )//neu an giu lau qua mot khoan thoi gian co nghia la touch bi loi, can reset touch
			{
				//count_reset_touch = TIME_NEED_TO_RESET_TOUCH;
				flag_reset_touch = 1; // bat len de reset touch
			}
			else
			{
				count_reset_touch++;
			}
			
			//HAL_Delay(100);
			if(old_pad1 == 0)
			{
				//reset count dem cho nhay
				count_nhay = 0;

				if(State_switch_1 == 0)
				{
					State_switch_1 = 1;
					buff_send_rf[1] = '1';
				}
				else
				{
					State_switch_1 = 0;
					buff_send_rf[1] = '0';
				}
				
				//count_update = TIME_NEED_UPDATE;
				count_update = 0;
				mcu_dp_bool_update(DPID_SWITCH_1,State_switch_1);
				/*#ifdef USE_WIFI 
				all_data_update();
				#else
				all_data_update();
				#endif*/
					
				old_pad1 = 1;
				has_change_touchpad = 1; // neu an nut thi set bien nay de truyen rf
				
				//tang count setup khi an nut dung
				if(old_button == 0 || old_button == 1 )
				{
					count_setup ++;
				}
				else
				{
					count_setup = 1;
				}
				old_button = 1;
			}
		}
		else
		{
			old_pad1 = 0;
		}
		
		//truong hop ko co nut nao an thi reset count_config_wifi = 0;
		if(TOUCH1_PAD1 != GPIO_PIN_SET)
		{
			count_config_wifi = 0;
			count_reset_touch = 0;
		}
		
		if( State_switch_1 == 1)   //relay
		{
			HAL_GPIO_WritePin(RELAY1_GPIO_Port,RELAY1_Pin,GPIO_PIN_SET);
			if(modeconfig == 0)
			{
				HAL_GPIO_WritePin(LED1_RED,GPIO_PIN_RESET);
				HAL_GPIO_WritePin(LED1_BLUE,GPIO_PIN_SET);
			}
		}
		else
		{
			HAL_GPIO_WritePin(RELAY1_GPIO_Port,RELAY1_Pin,GPIO_PIN_RESET);
			if(modeconfig == 0)
			{
				HAL_GPIO_WritePin(LED1_RED,GPIO_PIN_SET);
				HAL_GPIO_WritePin(LED1_BLUE,GPIO_PIN_RESET);
			}
		}
		
}
void wifi_init(void)
{
	HAL_GPIO_WritePin(ESP_RESET_GPIO_Port,ESP_RESET_Pin,GPIO_PIN_RESET);
	HAL_Delay(1);
	HAL_GPIO_WritePin(ESP_RESET_GPIO_Port,ESP_RESET_Pin,GPIO_PIN_SET);
	HAL_Delay(1000);
	HAL_UART_Receive_IT(&huart1,&Nhanbuff,1);
	
	
	#ifdef USE_WIFI 
	wifi_protocol_init();
	#else
	zigbee_protocol_init();
	#endif
	#ifdef USE_WIFI 
	//tiep tuc nhan du lieu
	HAL_GPIO_WritePin(RESET_ZIGBEE_GPIO_Port,RESET_ZIGBEE_Pin,GPIO_PIN_SET);
	HAL_UART_Receive_IT(&huart2,&Nhan_rf,1);
	
	//doc flash
	Read_HocLenh();
	#endif
}
