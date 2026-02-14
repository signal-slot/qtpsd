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
            height: 148,
            left: 26,
            top: 26,
            width: 148,
            child: Container(
              height: 145,
              width: 145,
              decoration: BoxDecoration(
                color: Color.fromARGB(255, 230, 230, 230),
                border: Border.all(
                  color: Color.fromARGB(255, 0, 0, 0),
                  width: 5,
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
