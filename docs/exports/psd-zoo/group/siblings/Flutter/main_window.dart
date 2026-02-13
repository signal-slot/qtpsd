import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 200,
      width: 200,
      child: Stack(
        children: [
          Positioned(
            height: 200,
            left: 0,
            top: 0,
            width: 200,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 200,
              width: 200,
            ),
          ),
          Container(
            child: Stack(
              children: [
                Positioned(
                  height: 40,
                  left: 0,
                  top: 0,
                  width: 40,
                  child: Image.asset(
                    "assets/images/layer_1.png", 
                    fit: BoxFit.contain,
                    height: 40,
                    width: 40,
                  ),
                ),
              ],
            ),
          ),
          Container(
            child: Stack(
              children: [
                Positioned(
                  height: 40,
                  left: 30,
                  top: 30,
                  width: 40,
                  child: Image.asset(
                    "assets/images/layer_2.png", 
                    fit: BoxFit.contain,
                    height: 40,
                    width: 40,
                  ),
                ),
              ],
            ),
          ),
          Container(
            child: Stack(
              children: [
                Positioned(
                  height: 40,
                  left: 60,
                  top: 60,
                  width: 40,
                  child: Image.asset(
                    "assets/images/layer_3.png", 
                    fit: BoxFit.contain,
                    height: 40,
                    width: 40,
                  ),
                ),
              ],
            ),
          ),
          Container(
            child: Stack(
              children: [
                Positioned(
                  height: 40,
                  left: 90,
                  top: 90,
                  width: 40,
                  child: Image.asset(
                    "assets/images/layer_4.png", 
                    fit: BoxFit.contain,
                    height: 40,
                    width: 40,
                  ),
                ),
              ],
            ),
          ),
          Container(
            child: Stack(
              children: [
                Positioned(
                  height: 40,
                  left: 120,
                  top: 120,
                  width: 40,
                  child: Image.asset(
                    "assets/images/layer_5.png", 
                    fit: BoxFit.contain,
                    height: 40,
                    width: 40,
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
