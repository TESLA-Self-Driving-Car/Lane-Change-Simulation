////
////  main.cpp
////  TestOpenGL
////
////  Created by HJKBD on 8/20/16.
////  Copyright © 2016 HJKBD. All rights reserved.
////
//
#include "picojson.h"
#include "layout.h"
#include "display.h"
#include "globals.h"
#include "model.h"
#include "search.h"
#include "inference.h"
#include "KdTree.hpp"
#include "decisionmaking2.h"
#include "util.h"

#include "ros/ros.h"
#include "traffic_msgs/VehicleState.h"
#include "traffic_msgs/VehicleStateArray.h"

#include <iostream>
#include <cmath>
#include <time.h>
#include <unistd.h>
#include <iomanip>
#include <fstream>

using namespace std;


GLFWwindow *window = NULL;

Display display = Display();
// ofstream myfile;

traffic_msgs::VehicleStateArray update_traffic_info(vector<Car*> cars) {
  Car* host;
  vector<Car*> cars_in_target_lane;
  for (Car* car: cars) {
    if (car->isHost()) { //my car moves
      cout << "***** Ego Car: *****" << endl;
      cout << "  position: " << car->getPos().x << ", " << car->getPos().y << endl;
      cout << "  velocity: " << car->getVelocity().x << ", " << car->getVelocity().y << endl;
      cout << "  dir: " << car->getDir().x << ", " << car->getDir().y << endl;
      host = car;
    }
  }

  for (Car* car: cars) {
    if (!car->isHost()) {
      if ( abs(car->getPos().y - host->getPos().y) > 0.5 ) {
        cars_in_target_lane.push_back(car);
      }
    }
  }

  sort(cars_in_target_lane.begin(), cars_in_target_lane.end(),
       [](Car* a, Car* b) { return a->getPos().x > b->getPos().x; });

  traffic_msgs::VehicleStateArray vehicle_array;

  cout << "+++++ Cars in the target lane: +++++" << endl;
  for (Car* car: cars_in_target_lane) {
    //other car moves
    cout << "  ------------" << endl;
    cout << "  position: " << car->getPos().x << ", " << car->getPos().y << endl;
    cout << "  velocity: " << car->getVelocity().x << ", " << car->getVelocity().y << endl;
    cout << "  dir: " << car->getDir().x << ", " << car->getDir().y << endl;

    traffic_msgs::VehicleState veh;
    veh.pose.pose.position.x = car->getPos().x;
    veh.pose.pose.position.y = car->getPos().y;
    veh.pose.pose.position.z = 0;
    veh.twist.twist.linear.x = car->getVelocity().x;
    veh.twist.twist.linear.y = car->getVelocity().y;
    vehicle_array.vehicles.push_back(veh);
  }

  cout << "+++++ Gaps in the target lane: +++++" << endl;
  for (int i = 0; i < cars_in_target_lane.size()-1; i++) {
    Car* leader = cars_in_target_lane[i];
    Car* follower = cars_in_target_lane[i+1];
    //other car moves
    cout << "  ------------" << endl;
    cout << "  gap size: " << leader->getPos().x - follower->getPos().x << endl;
    double gap_x = (leader->getPos().x + follower->getPos().x) / 2;
    double gap_y = (leader->getPos().y + follower->getPos().y) / 2;
    cout << "  gap center: " << gap_x << ", " << gap_y << endl;
    cout << "  distance to ego: " << gap_x - host->getPos().x << endl;
  }

  return vehicle_array;
}


int main(int argc, char **argv) {
  ros::init(argc, argv, "gap_selector");
  ros::NodeHandle n;

  ros::Publisher veh_array_pub = n.advertise<traffic_msgs::VehicleStateArray>("/traffic_info", 1000);

  ros::Rate rate(10);

  // myfile.open ("intention.txt"); // ?

  // load the map for running testing examples
  string worldname = "dense_traffic";
  Layout layout = Layout(worldname);
  Model model(layout);

  display.setColors(model.getCars());

  Car* mycar = model.getHost();
  cout << "***** Check host: *****" << endl;
  cout << mycar->isHost() << endl;
  cout << "***** Check mycar type: *****" << endl;
  cout << typeid(mycar).name() << endl;
  cout << endl;

  vector<Car*> cars = model.getCars();

  int SCREEN_WIDTH = layout.getWidth();
  int SCREEN_HEIGHT = layout.getHeight();
  string title = Globals::constant.TITLE;
  begin_graphics(SCREEN_WIDTH, SCREEN_HEIGHT, title);

  // loop util the user closes the window
  // bool gameover = false;

  bool over = false;
  DecisionAgent2 decision;
  vector<vec2f> mypath;
  vector<int> carintentions;

  for (int i = 0; i < model.getOtherCars().size(); i++) {
    carintentions.push_back(1);
  }

  bool success = decision.getPath(model, mypath, carintentions);

  // debug: check mypath
  cout << "***** Check mypath: *****" << endl;
  for(int i = 0; i < mypath.size(); i++) {
    cout << mypath[i].x << ", " << mypath[i].y << endl;
  }
  cout << endl;

  vector<vector<vec2f>> mypaths = decision.getPaths();
  // debug: check mypaths
  cout << "***** Check mypaths: *****" << endl;
  for(int i = 0; i < mypaths.size(); i++) {
    cout << "path " << i << endl;
    for(int j = 0; j < mypaths[0].size(); j++) {
      cout << mypaths[i][j].x << ", " << mypaths[i][j].y << endl;
    }
  }
  cout << endl;

  //pair<string, vec2f> actionset; // ?

  //string filename = "coop"; // ?

  // the change for car is mandatory
  bool change; // = true;
  srand(time(NULL));
  //int i = 0;

  while(ros::ok() && !glfwWindowShouldClose(window)) {
    glClearColor(1.0f, 1.0f, 1.0f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    Display::drawGoal(model.getFinish());
    Display::drawBlocks(model.getBlocks());
    Display::drawLine(model.getLine());

    // for(auto p : mypaths) {
    //   drawPolygon(p);
    // }
    for(int i = 0; i < mypaths.size(); i++) {
      drawPolygon(mypaths[i]);
    }

    display.drawCar(model.getHost());
    display.drawOtherCar(model.getOtherCars());

    if (!gameover(model)) {
      traffic_msgs::VehicleStateArray veh_array = update_traffic_info(cars);
      veh_array_pub.publish(veh_array);

      //update cars here for further processing
      for (Car* car: cars) {
        if (car == mycar) { //my car moves
          //destination reaches, generate new paths
          if (mypath.size() == 0 || abs(mycar->getPos().x - mypath[mypath.size()-1].x) < 10) {
            success = decision.getPath(model, mypath, carintentions);
            mypaths = decision.getPaths();
          }
          car->autonomousAction(mypath, model, NULL);
          car->update();
          drawPolygon(mypath);
        } else { //other car moves
          car->autonomousAction(mypath, model, NULL);
          car->update();
        }
      }
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
    over = gameover(model) || glfwWindowShouldClose(window);
    Display::sleep(0.05);
  }

  if (model.checkVictory()) {
    cout << "The car win" << endl;
  } else {
    cout << "You lose the car game" << endl;
  }

  glfwTerminate();
  // myfile.close();
  return 1;
}