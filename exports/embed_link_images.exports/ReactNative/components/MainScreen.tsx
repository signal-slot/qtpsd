import React from 'react';
import { View, StyleSheet, Image } from 'react-native';

const MainScreen: React.FC = () => {
  return (
    <View style={styles.root}>
      <View style={styles.style0}>
        <Image
          source={require('../assets/images/qtquick.png')}
          style={styles.style1}
          resizeMode='contain'
        />
        <Image
          source={require('../assets/images/qtquick_1.png')}
          style={styles.style2}
          resizeMode='contain'
        />
        <Image
          source={require('../assets/images/7c7c45584dc3695d.png')}
          style={styles.style3}
          resizeMode='contain'
        />
        <Image
          source={require('../assets/images/slint.png')}
          style={styles.style4}
          resizeMode='contain'
        />
        <Image
          source={require('../assets/images/55f66e81a5e64db8.png')}
          style={styles.style5}
          resizeMode='contain'
        />
        <Image
          source={require('../assets/images/flutter.png')}
          style={styles.style6}
          resizeMode='contain'
        />
        <Image
          source={require('../assets/images/flutter_pixeled.png')}
          style={styles.style7}
          resizeMode='contain'
        />
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  root: {
    flex: 1,
    width: 320,
    height: 240,
    backgroundColor: '#FFFFFF',
  },
  style0: {
    position: 'absolute',
    left: 0,
    top: 0,
    backgroundColor: '#F9CFF4',
  },
  style1: {
    position: 'absolute',
    left: 40,
    top: 80,
    width: 64,
    height: 46,
  },
  style2: {
    position: 'absolute',
    left: 58,
    top: 16,
    width: 36,
    height: 48,
  },
  style3: {
    position: 'absolute',
    left: 40,
    top: 150,
    width: 64,
    height: 46,
  },
  style4: {
    position: 'absolute',
    left: 150,
    top: 79,
    width: 36,
    height: 48,
  },
  style5: {
    position: 'absolute',
    left: 150,
    top: 149,
    width: 36,
    height: 48,
  },
  style6: {
    position: 'absolute',
    left: 240,
    top: 82,
    width: 32,
    height: 40,
  },
  style7: {
    position: 'absolute',
    left: 240,
    top: 152,
    width: 32,
    height: 40,
  },
});

export default MainScreen;
