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
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 300,
              width: 300,
            ),
          ),
          Positioned(
            height: 19,
            left: 10,
            top: 5,
            width: 45,
            child: Text(
              "Layer 1", 
              textAlign: TextAlign.left,
              textScaler: TextScaler.linear(1),
              style: TextStyle(
                color: Color.fromARGB(255, 255, 0, 0),
                fontFamily: "Roboto",
                fontSize: 14,
                fontVariations: [FontVariation.weight(600)],
                height: 1,
              ),
            ),
          ),
          Positioned(
            height: 19,
            left: 10,
            top: 30,
            width: 45,
            child: Text(
              "Layer 2", 
              textAlign: TextAlign.left,
              textScaler: TextScaler.linear(1),
              style: TextStyle(
                color: Color.fromARGB(255, 0, 255, 0),
                fontFamily: "Roboto",
                fontSize: 14,
                fontVariations: [FontVariation.weight(600)],
                height: 1,
              ),
            ),
          ),
          Positioned(
            height: 19,
            left: 10,
            top: 55,
            width: 45,
            child: Text(
              "Layer 3", 
              textAlign: TextAlign.left,
              textScaler: TextScaler.linear(1),
              style: TextStyle(
                color: Color.fromARGB(255, 0, 0, 255),
                fontFamily: "Roboto",
                fontSize: 14,
                fontVariations: [FontVariation.weight(600)],
                height: 1,
              ),
            ),
          ),
          Positioned(
            height: 19,
            left: 10,
            top: 80,
            width: 45,
            child: Text(
              "Layer 4", 
              textAlign: TextAlign.left,
              textScaler: TextScaler.linear(1),
              style: TextStyle(
                color: Color.fromARGB(255, 255, 128, 0),
                fontFamily: "Roboto",
                fontSize: 14,
                fontVariations: [FontVariation.weight(600)],
                height: 1,
              ),
            ),
          ),
          Positioned(
            height: 19,
            left: 10,
            top: 105,
            width: 45,
            child: Text(
              "Layer 5", 
              textAlign: TextAlign.left,
              textScaler: TextScaler.linear(1),
              style: TextStyle(
                color: Color.fromARGB(255, 128, 0, 255),
                fontFamily: "Roboto",
                fontSize: 14,
                fontVariations: [FontVariation.weight(600)],
                height: 1,
              ),
            ),
          ),
          Positioned(
            height: 19,
            left: 10,
            top: 130,
            width: 45,
            child: Text(
              "Layer 6", 
              textAlign: TextAlign.left,
              textScaler: TextScaler.linear(1),
              style: TextStyle(
                color: Color.fromARGB(255, 0, 128, 128),
                fontFamily: "Roboto",
                fontSize: 14,
                fontVariations: [FontVariation.weight(600)],
                height: 1,
              ),
            ),
          ),
          Positioned(
            height: 19,
            left: 10,
            top: 155,
            width: 45,
            child: Text(
              "Layer 7", 
              textAlign: TextAlign.left,
              textScaler: TextScaler.linear(1),
              style: TextStyle(
                color: Color.fromARGB(255, 128, 128, 0),
                fontFamily: "Roboto",
                fontSize: 14,
                fontVariations: [FontVariation.weight(600)],
                height: 1,
              ),
            ),
          ),
          Positioned(
            height: 19,
            left: 10,
            top: 180,
            width: 45,
            child: Text(
              "Layer 8", 
              textAlign: TextAlign.left,
              textScaler: TextScaler.linear(1),
              style: TextStyle(
                color: Color.fromARGB(255, 64, 64, 64),
                fontFamily: "Roboto",
                fontSize: 14,
                fontVariations: [FontVariation.weight(600)],
                height: 1,
              ),
            ),
          ),
          Positioned(
            height: 19,
            left: 10,
            top: 205,
            width: 45,
            child: Text(
              "Layer 9", 
              textAlign: TextAlign.left,
              textScaler: TextScaler.linear(1),
              style: TextStyle(
                color: Color.fromARGB(255, 192, 0, 192),
                fontFamily: "Roboto",
                fontSize: 14,
                fontVariations: [FontVariation.weight(600)],
                height: 1,
              ),
            ),
          ),
          Positioned(
            height: 19,
            left: 10,
            top: 230,
            width: 53,
            child: Text(
              "Layer 10", 
              textAlign: TextAlign.left,
              textScaler: TextScaler.linear(1),
              style: TextStyle(
                color: Color.fromARGB(255, 0, 192, 0),
                fontFamily: "Roboto",
                fontSize: 14,
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
