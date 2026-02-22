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
            child: Container(
              decoration: (
                color: Color.fromARGB(255, 0, 0, 0),
              ),
            ),
          ),
          Positioned(
            height: 48,
            left: 50,
            top: 82,
            width: 168,
            child: Text(
              "White Text", 
              textAlign: TextAlign.left,
              textScaler: TextScaler.linear(1),
              style: TextStyle(
                color: Color.fromARGB(255, 255, 255, 255),
                fontFamily: "Roboto",
                fontSize: 36,
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
