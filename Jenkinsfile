pipeline {
    agent any

    stages {
        stage('Checkout') {
            steps {
                checkout scm
            }
        }
        
        stage('Build Firmware') {
            steps {
                powershell '''
                    $env:IDF_TOOLS_PATH = "C:\\Espressif\\tools"
                    $env:IDF_PYTHON_ENV_PATH = "C:\\Espressif\\tools\\python\\v6.0.1\\venv"
                    $env:IDF_PATH = "C:\\esp\\v6.0.1\\esp-idf"
                    
                    . $env:IDF_PATH\\export.ps1
                    
                    idf.py build
                '''
            }
        }
    }
    
    post {
        success {
            archiveArtifacts artifacts: 'build/ota_node.bin', fingerprint: true
        }
    }
}