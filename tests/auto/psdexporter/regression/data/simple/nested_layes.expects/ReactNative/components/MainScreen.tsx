import React from 'react';
import { View, StyleSheet, Text } from 'react-native';

const MainScreen: React.FC = () => {
  return (
    <View style={styles.root}>
      <View style={styles.style0}>
        <View style={styles.style1}>
          <View style={styles.style2} />
          <View style={styles.style3}>
            <View style={styles.style4} />
            <View style={styles.style5} />
          </View>
          <Text style={styles.style6}>Example1</Text>
        </View>
        <View style={styles.style7}>
          <View style={styles.style8} />
          <View style={styles.style9}>
            <View style={styles.style10} />
            <View style={styles.style11} />
          </View>
          <Text style={styles.style12}>Example1</Text>
        </View>
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
    backgroundColor: '#F7E790',
  },
  style1: {
    position: 'absolute',
    left: 0,
    top: 0,
  },
  style10: {
    position: 'absolute',
    left: 72,
    top: 174,
    width: 139,
    height: 12,
    backgroundColor: '#217903',
  },
  style11: {
    position: 'absolute',
    left: 179,
    top: 166,
    width: 25,
    height: 14,
    backgroundColor: '#5E88E7',
  },
  style12: {
    position: 'absolute',
    left: 50,
    top: 143,
    width: 160,
    height: 43,
    fontFamily: '源ノ角ゴシック JP',
    fontSize: 30,
    color: '#000000',
    textAlign: 'center',
  },
  style2: {
    position: 'absolute',
    left: 39,
    top: 49,
    width: 182,
    height: 52,
    backgroundColor: '#D4F5C9',
    borderRadius: 20,
  },
  style3: {
    position: 'absolute',
    left: 0,
    top: 0,
  },
  style4: {
    position: 'absolute',
    left: 72,
    top: 89,
    width: 139,
    height: 13,
    backgroundColor: '#217903',
  },
  style5: {
    position: 'absolute',
    left: 79,
    top: 81,
    width: 25,
    height: 14,
    backgroundColor: '#5E88E7',
  },
  style6: {
    position: 'absolute',
    left: 50,
    top: 63,
    width: 160,
    height: 43,
    fontFamily: '源ノ角ゴシック JP',
    fontSize: 30,
    color: '#000000',
    textAlign: 'center',
  },
  style7: {
    position: 'absolute',
    left: 0,
    top: 0,
  },
  style8: {
    position: 'absolute',
    left: 39,
    top: 134,
    width: 182,
    height: 52,
    backgroundColor: '#D4F5C9',
    borderRadius: 20,
  },
  style9: {
    position: 'absolute',
    left: 0,
    top: 0,
  },
});

export default MainScreen;
