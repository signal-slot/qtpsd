import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 200,
      width: 300,
      child: Stack(
        children: [
          Positioned(
            height: 200,
            left: 0,
            top: 0,
            width: 300,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 200,
              width: 300,
            ),
          ),
          Container(
            child: Stack(
              children: [
                Positioned(
                  height: 200,
                  left: 0,
                  top: 0,
                  width: 60,
                  child: Image.asset(
                    "assets/images/layer_1.png", 
                    fit: BoxFit.contain,
                    height: 200,
                    width: 60,
                  ),
                ),
              ],
            ),
          ),
          Container(
            child: Stack(
              children: [
                Positioned(
                  height: 200,
                  left: 60,
                  top: 0,
                  width: 60,
                  child: Image.asset(
                    "assets/images/layer_2.png", 
                    fit: BoxFit.contain,
                    height: 200,
                    width: 60,
                  ),
                ),
              ],
            ),
          ),
          Container(
            child: Stack(
              children: [
                Positioned(
                  height: 200,
                  left: 120,
                  top: 0,
                  width: 60,
                  child: Image.asset(
                    "assets/images/layer_3.png", 
                    fit: BoxFit.contain,
                    height: 200,
                    width: 60,
                  ),
                ),
              ],
            ),
          ),
          Container(
            child: Stack(
              children: [
                Positioned(
                  height: 200,
                  left: 180,
                  top: 0,
                  width: 60,
                  child: Image.asset(
                    "assets/images/layer_4.png", 
                    fit: BoxFit.contain,
                    height: 200,
                    width: 60,
                  ),
                ),
              ],
            ),
          ),
          Container(
            child: Stack(
              children: [
                Positioned(
                  height: 200,
                  left: 240,
                  top: 0,
                  width: 60,
                  child: Image.asset(
                    "assets/images/layer_5.png", 
                    fit: BoxFit.contain,
                    height: 200,
                    width: 60,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
