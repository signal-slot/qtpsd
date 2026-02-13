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
            height: 100,
            left: 50,
            top: 50,
            width: 200,
            child: Image.asset(
              "assets/images/multi_effect.png", 
              fit: BoxFit.contain,
              height: 100,
              width: 200,
            ),
          ),
        ],
      ),
    );
  }
}
