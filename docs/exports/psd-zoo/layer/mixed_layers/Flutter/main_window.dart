import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 300,
      width: 300,
      child: Stack(
        children: [
          Positioned(
            height: 300,
            left: 0,
            top: 0,
            width: 300,
            child: Container(
              decoration: (
                color: Color.fromARGB(255, 0, 0, 0),
              ),
            ),
          ),
          Text(
            "", 
            textAlign: TextAlign.left,
            textScaler: TextScaler.linear(1),
            style: TextStyle(
              color: Color.fromARGB(255, 0, 0, 0),
              fontFamily: "Sans Serif",
              fontSize: 9,
              fontVariations: [FontVariation.weight(600)],
              height: 1,
            ),
          ),
        ],
      ),
    );
  }
}
