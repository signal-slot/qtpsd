import { StatusBar } from 'expo-status-bar';
import { ScrollView, StyleSheet, Text } from 'react-native';
// Import your exported component here:
// import MainScreen from './components/MainScreen';

export default function App() {
  return (
    <ScrollView style={styles.container} contentContainerStyle={styles.content}>
      {/* Replace this with your exported component */}
      <Text>Copy exported .tsx to components/ and images to assets/images/</Text>
      <StatusBar style="auto" />
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#fff',
  },
  content: {
    alignItems: 'center',
    justifyContent: 'center',
    padding: 20,
  },
});
