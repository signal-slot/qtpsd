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
            height: 47,
            left: 35,
            top: 93,
            width: 260,
            child: Image.asset(
              "assets/images/shelllower.png", 
              fit: BoxFit.contain,
              height: 47,
              width: 260,
            ),
          ),
        ],
      ),
    );
  }
}
