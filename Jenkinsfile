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
                    # Tell ESP-IDF exactly where the tools are installed globally
                    $env:IDF_TOOLS_PATH = "C:\\Espressif\\tools"
                    
                    # Ensure the bootstrap Python is available to the Jenkins service
                    $env:PATH = "C:\\Espressif\\tools\\python\\v6.0.1\\venv\\Scripts;" + $env:PATH
                    
                    # Set the IDF path and execute the export script
                    $env:IDF_PATH = "C:\\esp\\v6.0.1\\esp-idf"
                    . $env:IDF_PATH\\export.ps1
                    
                    # Build the firmware
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