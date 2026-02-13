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
          Positioned(
            height: 200,
            left: 0,
            top: 0,
            width: 200,
            child: Image.asset(
              "assets/images/bottom_red.png", 
              fit: BoxFit.contain,
              height: 200,
              width: 200,
            ),
          ),
          Positioned(
            height: 200,
            left: 20,
            top: 20,
            width: 200,
            child: Image.asset(
              "assets/images/middle_green.png", 
              fit: BoxFit.contain,
              height: 200,
              width: 200,
            ),
          ),
          Positioned(
            height: 200,
            left: 40,
            top: 40,
            width: 200,
            child: Image.asset(
              "assets/images/top_blue.png", 
              fit: BoxFit.contain,
              height: 200,
              width: 200,
            ),
          ),
        ],
      ),
    );
  }
}
