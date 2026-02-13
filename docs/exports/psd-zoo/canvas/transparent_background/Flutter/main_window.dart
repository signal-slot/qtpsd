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
          Image.asset(
            "assets/images/", 
            fit: BoxFit.contain,
            height: 0,
            width: 0,
          ),
          Positioned(
            height: 100,
            left: 50,
            top: 50,
            width: 100,
            child: Image.asset(
              "assets/images/floating_square.png", 
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
