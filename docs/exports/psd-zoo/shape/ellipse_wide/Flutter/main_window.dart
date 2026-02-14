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
            height: 102,
            left: 19,
            top: 49,
            width: 262,
            child: Container(
              decoration: BoxDecoration(
                borderRadius: BorderRadius.circular(130),
                color: Color.fromARGB(255, 255, 128, 0),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
