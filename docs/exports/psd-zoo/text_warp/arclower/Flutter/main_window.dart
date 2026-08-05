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
            height: 56,
            left: 50,
            top: 95,
            width: 189,
            child: Image.asset(
              "assets/images/arclower.png", 
              fit: BoxFit.contain,
              height: 56,
              width: 189,
            ),
          ),
        ],
      ),
    );
  }
}
