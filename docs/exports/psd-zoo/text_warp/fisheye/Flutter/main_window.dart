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
            height: 32,
            left: 50,
            top: 91,
            width: 141,
            child: Image.asset(
              "assets/images/fisheye.png", 
              fit: BoxFit.contain,
              height: 32,
              width: 141,
            ),
          ),
        ],
      ),
    );
  }
}
