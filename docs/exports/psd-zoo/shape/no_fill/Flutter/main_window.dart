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
            height: 146,
            left: 27,
            top: 27,
            width: 146,
            child: Container(
              height: 143,
              width: 143,
              decoration: BoxDecoration(
                color: Color.fromARGB(255, 0, 0, 0),
                border: Border.all(
                  color: Color.fromARGB(255, 255, 0, 0),
                  width: 3,
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
