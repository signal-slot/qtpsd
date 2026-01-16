import React from 'react';
import { View, StyleSheet } from 'react-native';
import Svg, { Path } from 'react-native-svg';

const MainScreen: React.FC = () => {
  return (
    <View style={styles.root}>
      <View style={styles.style0}>
        <Svg width={131} height={93} style={styles.style1}>
          <Path
            d="M 66.5 2 C 66.5 2 119.027 20.4198 119.027 20.4198 C 119.027 20.4198 132 61.8087 132 61.8087 C 132 61.8087 95.6502 95 95.6502 95 C 95.6502 95 37.3498 95 37.3498 95 C 37.3498 95 1 61.8087 1 61.8087 C 1 61.8087 13.9731 20.4198 13.9731 20.4198 C 13.9731 20.4198 66.5 2 66.5 2 Z"
            fill="#5A6F0E"
          />
        </Svg>
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
    backgroundColor: '#8FC5F3',
  },
  style1: {
    position: 'absolute',
    left: 64,
    top: 74,
    width: 134,
    height: 96,
    opacity: 0.368627,
  },
});

export default MainScreen;
