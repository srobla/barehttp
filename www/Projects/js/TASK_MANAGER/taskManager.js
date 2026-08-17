// element.classList.add("mystyle");
//parent.removeChild(child);
var taskTemplate = document.getElementById("taskTemplate");
var taskGroupTemplate = document.getElementById("taskGroupTemplate");

function checkTask(button){
    task = button.parentElement;
    const container = task.parentElement;
    container.append(task);
    task.classList.add("checked")
    task.classList.remove("unchecked")
}

function uncheckTask(button){
    task = button.parentElement;
    const container = task.parentElement;
    container.prepend(task);
    task.classList.add("unchecked")
    task.classList.remove("checked")
}

function deleteTask(button){
    const container = button.parentElement.parentElement;
    task = button.parentElement;
    task.parentElement.removeChild(task);
    if(container.childElementCount <= 17){
        document.getElementById("addTask").style.display = "block";
    }
}

function addTask(button){
    const container = button.parentElement.parentElement;
    const clone = taskTemplate.content.cloneNode(true);
    const task = clone.getElementById("task");
    container.prepend(task);
    if(container.childElementCount > 17){
        document.getElementById("addTask").style.display = "none";
    }
}

function addTaskGroup(button){
    const container = button.parentElement.parentElement;
    const clone = taskGroupTemplate.content.cloneNode(true);
    const taskGroup = clone.getElementById("taskGroup");
    container.prepend(taskGroup);
}

function deleteTaskGroup(button){
    const taskGroup = button.parentElement;
    taskGroup.parentElement.removeChild(taskGroup);
}