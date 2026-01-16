import React from 'react';
import { View, StyleSheet } from 'react-native';
import Svg, { Path } from 'react-native-svg';

const MainScreen: React.FC = () => {
  return (
    <View style={styles.root}>
      <View style={styles.style0}>
        <Svg width={131} height={93} style={styles.style1}>
          <Path
            d="M 76.7117 11.0005 C 76.7117 11.0005 129.154 29.6605 129.154 29.6605 C 129.154 29.6605 141.937 71.1084 141.937 71.1084 C 141.937 71.1084 105.436 104.133 105.436 104.133 C 105.436 104.133 47.1361 103.866 47.1361 103.866 C 47.1361 103.866 10.9386 70.5087 10.9386 70.5087 C 10.9386 70.5087 24.101 29.1796 24.101 29.1796 C 24.101 29.1796 76.7117 11.0005 76.7117 11.0005 Z"
            fill="#5A6F0E"
            stroke="#E5FC14"
            strokeWidth={17}
          />
        </Svg>
        <Svg width={131} height={93} style={styles.style3}>
          <Path
            d="M 66.5 0.999999 C 66.5 0.999999 119.027 19.4198 119.027 19.4198 C 119.027 19.4198 132 60.8087 132 60.8087 C 132 60.8087 95.6502 94 95.6502 94 C 95.6502 94 37.3498 94 37.3498 94 C 37.3498 94 1 60.8087 1 60.8087 C 1 60.8087 13.9731 19.4198 13.9731 19.4198 C 13.9731 19.4198 66.5 0.999999 66.5 0.999999 Z"
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
    left: 54,
    top: 65,
    width: 153,
    height: 114,
    opacity: 0.368627,
  },
  style3: {
    position: 'absolute',
    left: 170,
    top: 75,
    width: 133,
    height: 95,
    opacity: 0.368627,
  },
});

export default MainScreen;
