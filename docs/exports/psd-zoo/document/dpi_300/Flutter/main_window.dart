import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 600,
      width: 600,
      child: Stack(
        children: [
          Positioned(
            height: 600,
            left: 0,
            top: 0,
            width: 600,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 600,
              width: 600,
            ),
          ),
          Positioned(
            height: 66,
            left: 50,
            top: 48,
            width: 175,
            child: Text(
              "300 DPI", 
              textAlign: TextAlign.left,
              textScaler: TextScaler.linear(1),
              style: TextStyle(
                color: Color.fromARGB(255, 0, 0, 0),
                fontFamily: "Roboto",
                fontSize: 50,
                fontVariations: [FontVariation.weight(600)],
                height: 1,
              ),
            ),
          ),
        ],
      ),
    );
  }
}
