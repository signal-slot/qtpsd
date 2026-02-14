import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 300,
      width: 400,
      child: Stack(
        children: [
          Positioned(
            height: 300,
            left: 0,
            top: 0,
            width: 400,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 300,
              width: 400,
            ),
          ),
          Positioned(
            height: 100,
            left: 50,
            top: 50,
            width: 100,
            child: Image.asset(
              "assets/images/raster_circle.png", 
              fit: BoxFit.contain,
              height: 100,
              width: 100,
            ),
          ),
          Positioned(
            height: 37,
            left: 50,
            top: 221,
            width: 183,
            child: Text(
              "Mixed Content", 
              textAlign: TextAlign.left,
              textScaler: TextScaler.linear(1),
              style: TextStyle(
                color: Color.fromARGB(255, 0, 0, 0),
                fontFamily: "Roboto",
                fontSize: 28,
                fontVariations: [FontVariation.weight(600)],
                height: 1,
              ),
            ),
          ),
          Positioned(
            height: 152,
            left: 199,
            top: 49,
            width: 172,
            child: Container(
              decoration: BoxDecoration(
                color: Color.fromARGB(255, 0, 100, 200),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
