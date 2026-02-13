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
          Positioned(
            height: 140,
            left: 10,
            top: 30,
            width: 80,
            child: Image.asset(
              "assets/images/styled_1.png", 
              fit: BoxFit.contain,
              height: 140,
              width: 80,
            ),
          ),
          Positioned(
            height: 140,
            left: 100,
            top: 30,
            width: 80,
            child: Image.asset(
              "assets/images/styled_2.png", 
              fit: BoxFit.contain,
              height: 140,
              width: 80,
            ),
          ),
          Positioned(
            height: 140,
            left: 190,
            top: 30,
            width: 80,
            child: Image.asset(
              "assets/images/styled_3.png", 
              fit: BoxFit.contain,
              height: 140,
              width: 80,
            ),
          ),
        ],
      ),
    );
  }
}
