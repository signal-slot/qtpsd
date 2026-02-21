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
            child: Container(
              decoration: BoxDecoration(
                gradient: RadialGradient(
                  center: Alignment(0, 0),
                  colors: [
                    Color.fromARGB(255, 255, 0, 0),
                    Color.fromARGB(255, 0, 0, 255),
                  ],
                  radius: 0.5,
                  stops: [
                    0,
                    1,
                  ],
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
