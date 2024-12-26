from flask import Flask, request, jsonify
import pickle
import numpy as np
from sklearn.preprocessing import StandardScaler

app = Flask(__name__)

# Load the trained model
with open('activity_model.pkl', 'rb') as model_file:
    model = pickle.load(model_file)

# Initialize the scaler (assuming the scaler was fitted during training)
scaler = StandardScaler()
# You can also save the scaler to a file and load it here

@app.route('/predict', methods=['POST'])
def predict():
    try:
        data = request.get_json()

        
        if 'gyro_x' not in data or 'gyro_y' not in data or 'gyro_z' not in data:
            return jsonify({'error': 'Invalid input data'}), 400

       
        input_data = np.array([[data['gyro_x'], data['gyro_y'], data['gyro_z']]])
        
        input_data_scaled = scaler.fit_transform(input_data)

        prediction = model.predict(input_data_scaled)

        return jsonify({'predicted_activity': prediction[0]})

    except Exception as e:
        return jsonify({'error': str(e)}), 500

if __name__ == '__main__':
    app.run(debug=True)
