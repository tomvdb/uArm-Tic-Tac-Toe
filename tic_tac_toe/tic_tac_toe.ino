/******************************************************************************************
  File Name          : tic_tac_toe.ino
  Author             : Dave Corboy
  Version            : V1.0
  Date               : 22 Dec, 2015
  Modified Date      : 25 Dec, 2015
  Description        : uArm Metal Tic-Tac-Toe player
  Copyright(C) 2015 Dave Corboy. All right reserved.
*******************************************************************************************/

//#include <EEPROM.h>
#include <Wire.h>
#include <Servo.h>
#include "gameboard.h"
#include "gamelogic.h"
#include "sensor.h"
#include "uarm.h"

#define WAIT_READY    0   // the game control states
#define WAIT_START    1
#define PLAYER_TURN   2
#define UARM_TURN     3
#define POSTGAME      4
#define DECODE_BOARD  5
#define STABLE_BOARD  6
#define DEBUG         7
#define PICKUP_TESTS  8

GameBoard board;
GameLogic logic(&board);
Sensor sensor(&board);
uArm_Controller uarm_ctrl;

byte state;
byte player_mark;

void setup() {
  Wire.begin();        // join i2c bus (address optional for master)
  Serial.begin(9600);  // start serial port at 9600 bps
  sensor.begin();
  uarm_ctrl.begin();
  delay(500);
  change_state(WAIT_READY);
}


void loop() {
  byte move;
  byte input = NO_VAL;
  if (Serial.available() > 0) {
    input = Serial.read();
  }
  switch (state) {
    case WAIT_READY :
      if (sensor.board_ready() || input == 'r') {
        change_state(WAIT_START);
      } else if (input == 'd') {
        change_state(DEBUG);
      }
      break;
    case WAIT_START :
      {
        byte player_first = sensor.detect_start();
        if (player_first != NO_VAL || input != NO_VAL) {
          if (input == 'f') {
            player_first = true;
          } else if (input == 's') {
            player_first = false;
          }
          Serial.println(F("Starting game"));
          start_game(player_first);
        }
      }
      break;
    case PLAYER_TURN :
      {
        move = sensor.detect_player_move();
        if (input != NO_VAL) {
          byte num = input - '0';
          if (num >= 1 && num <= 9) {
            if (board.valid_move(num - 1)) {
              move = num - 1;
            } else {
              Serial.println(F("Move is not valid"));
            }
          }
        }
        if (move != NO_VAL) {
          board.set_posn(move);
          Serial.print(F("Player moves to: "));
          Serial.println(move + 1);
          change_state(board.game_over() ? POSTGAME : UARM_TURN);
        } else if (input == 'q') {
          change_state(POSTGAME);
        }
      }
      break;
    case UARM_TURN :
      move = logic.do_move();
      board.set_posn(move);
      Serial.print(F("UArm moves to: "));
      Serial.println(move + 1);
      uarm_ctrl.make_move(move);
      change_state(board.game_over() ? POSTGAME : PLAYER_TURN);
      break;
    case POSTGAME :
      if (input == 's') {
        change_state(WAIT_READY);
      }
      break;
    case DEBUG :
      if (input == 'q') {
        change_state(WAIT_READY);
      } else if (input == 'x') {
        uarm_ctrl.show_board_position(X_MARKER_POS);
      } else if (input == 'o') {
        uarm_ctrl.show_board_position(O_MARKER_POS);
      } else if (input == 'w') {
        uarm_ctrl.show_board_position(WAIT_POS);
      } else if (input == 'd') {
        change_state(DECODE_BOARD);
      } else if (input == 's') {
        change_state(STABLE_BOARD);
      } else if (input == 'c') {
        uarm_ctrl.show_xyz();
      } else if (input == 'p') {
        change_state(PICKUP_TESTS);
      } else if (input == 'l') {
        if (digitalRead(STOPPER) == HIGH) {
          Serial.println(F("HIGH"));
        } else {
          Serial.println(F("LOW"));
        }
      } else if (input != NO_VAL) {
        byte num = input - '0';
        if (num >= 1 && num <= 9) {
          uarm_ctrl.show_board_position(num - 1);
        }
      }
      break;
    case DECODE_BOARD :
      {
        byte board[9];
        if (sensor.check_board(board)) {
          print_board(board);
          change_state(DEBUG);
        }
      }
      break;
    case STABLE_BOARD :
      {
        byte board[9];
        if (sensor.valid_board(board)) {
          print_board(board);
          change_state(DEBUG);
        }
      }
      break;
    case PICKUP_TESTS :
      if (input == 'q') {
        change_state(DEBUG);
      } else if (input == 'l') {
        if (digitalRead(STOPPER) == HIGH) {
          Serial.println(F("HIGH"));
        } else {
          Serial.println(F("LOW"));
        }
      } else if (input == 'd') {
        uarm_ctrl.down_to_touch();
      } else if (input == 'u') {
        uarm_ctrl.show_board_position(WAIT_POS);
      } else if (input == 'a') {
        uarm_ctrl.attach_release(true);
      } else if (input == 'r') {
        uarm_ctrl.attach_release(false);
      } else if (input == 'm') {
        uarm_ctrl.move_marker(-7, -17, 8, 7, -17, 8);
      } else if (input == 'b') {
        uarm_ctrl.make_move(5);
        uarm_ctrl.make_move(6);
        uarm_ctrl.make_move(8);
      }
      break;
  }
}

void start_game(bool player_first) {
  //Serial.println(F("The game has almost begun"));
  logic.new_game(!player_first, MODE_EASY);
  uarm_ctrl.new_game(!player_first);
  player_mark = player_first ? 1 : 2;
  Serial.println(F("The game has begun"));
  change_state(player_first ? PLAYER_TURN : UARM_TURN);
}

void change_state(byte new_state) {
  switch (new_state) {
    case WAIT_READY :
      board.reset();
      sensor.reset();
      Serial.println(F("Waiting for board to be (R)eady... (or (D)ebug)"));
      uarm_ctrl.wait_ready();
      break;
    case WAIT_START :
      Serial.println(F("Waiting for you to go (F)irst, unless you want to go (S)econd"));
      uarm_ctrl.wait_start();
      break;
    case PLAYER_TURN :
      uarm_ctrl.wait_player();
      Serial.println(F("Waiting for player move (or 1..9)"));
      print_board(board.get_board());
      break;
    case UARM_TURN :
      break;
    case POSTGAME :
      {
        byte winner = board.winner();
        Serial.println(F("The game is over  (S)kip"));
        print_board(board.get_board());
        if (winner != 0) {
          Serial.print(winner == player_mark ? F("Player") : F("uArm"));
          Serial.println(F(" is the winner!"));
        } else {
          Serial.println(F("The game is a draw..."));
        }
        uarm_ctrl.postgame(winner);
        break;
      }
    case DEBUG :
      Serial.println(F("(R)eset, (D)ecode board, (S)table board, Board Positions (1-9)"));
      Serial.println(F("(X)-Markers, (O)-Markers, (W)ait position, (C)urrent XYZ"));
      Serial.println(F("(P)ickup tests or (Q)uit"));
      break;
    case PICKUP_TESTS :
      Serial.println(F("(L)imit switch, (D)own to touch, (U)p to lift"));
      Serial.println(F("(A)ttach, (R)elease, (M)ove 4-6 (B)ig Enchilada or (Q)uit"));
      break;
  }
  state = new_state;
}

void print_board(byte board[]) {
  char buf[32];
  sprintf(buf, "  %c | %c | %c\n", get_mark(board[0]), get_mark(board[1]), get_mark(board[2]));
  Serial.print(buf);
  Serial.println(" -----------");
  sprintf(buf, "  %c | %c | %c\n", get_mark(board[3]), get_mark(board[4]), get_mark(board[5]));
  Serial.print(buf);
  Serial.println(" -----------");
  sprintf(buf, "  %c | %c | %c\n", get_mark(board[6]), get_mark(board[7]), get_mark(board[8]));
  Serial.print(buf);
}

char get_mark(byte val) {
  switch (val) {
    case 0 :
      return ' ';
      break;
    case 1 :
      return 'X';
      break;
    case 2 :
      return 'O';
      break;
    default :
      return '?';
      break;
  }
}

//    if (readSerial == 'h') // Single Quote! This is a character.
//    {
//      digitalWrite(led, HIGH);
//      Serial.print(" LED ON ");
//    }
//
//    if (readSerial == 'l')
//    {
//      digitalWrite(led, LOW);
//      Serial.print(" LED OFF");
//    }

