import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 240,
      width: 320,
      child: Stack(
        children: [
          Container(
            color: Color.fromARGB(255, 249, 207, 244),
            height: 240,
            width: 320,
            child: Stack(
              children: [
                Positioned(
                  height: 46,
                  left: 40,
                  top: 80,
                  width: 64,
                  child: Image.asset(
                    "assets/images/qtquick.png", 
                    fit: BoxFit.contain,
                    height: 46,
                    width: 64,
                  ),
                ),
                Positioned(
                  height: 48,
                  left: 58,
                  top: 16,
                  width: 36,
                  child: Image.asset(
                    "assets/images/qtquick_1.png", 
                    fit: BoxFit.contain,
                    height: 48,
                    width: 36,
                  ),
                ),
                Positioned(
                  height: 46,
                  left: 40,
                  top: 150,
                  width: 64,
                  child: Image.asset(
                    "assets/images/7c7c45584dc3695d1d67a26219dfced93dd2c180e564f292def583b27ba9ecbd.png", 
                    fit: BoxFit.contain,
                    height: 46,
                    width: 64,
                  ),
                ),
                Positioned(
                  height: 48,
                  left: 150,
                  top: 79,
                  width: 36,
                  child: Image.asset(
                    "assets/images/slint.png", 
                    fit: BoxFit.contain,
                    height: 48,
                    width: 36,
                  ),
                ),
                Positioned(
                  height: 48,
                  left: 150,
                  top: 149,
                  width: 36,
                  child: Image.asset(
                    "assets/images/55f66e81a5e64db810dddb2ca1ea18b8f4b08b1d3946916d5e5a78e453772487.png", 
                    fit: BoxFit.contain,
                    height: 48,
                    width: 36,
                  ),
                ),
                Positioned(
                  height: 40,
                  left: 240,
                  top: 82,
                  width: 32,
                  child: Image.asset(
                    "assets/images/flutter.png", 
                    fit: BoxFit.contain,
                    height: 40,
                    width: 32,
                  ),
                ),
                Positioned(
                  height: 40,
                  left: 240,
                  top: 152,
                  width: 32,
                  child: Image.asset(
                    "assets/images/flutter_pixeled.png", 
                    fit: BoxFit.contain,
                    height: 40,
                    width: 32,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
