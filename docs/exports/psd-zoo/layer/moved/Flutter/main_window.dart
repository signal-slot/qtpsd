import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 300,
      width: 300,
      child: Stack(
        children: [
          Positioned(
            height: 300,
            left: 0,
            top: 0,
            width: 300,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 300,
              width: 300,
            ),
          ),
          Positioned(
            height: 100,
            left: 100,
            top: 150,
            width: 100,
            child: Image.asset(
              "assets/images/moved.png", 
              fit: BoxFit.contain,
              height: 100,
              width: 100,
            ),
          ),
        ],
      ),
    );
  }
}
