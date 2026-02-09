function showToast(message, type) {
	type = type || 'danger';
	var container = document.querySelector('.toast-container');
	if (!container) {
		container = document.createElement('div');
		container.className = 'toast-container position-fixed top-0 end-0 p-3';
		container.style.zIndex = '1080';
		document.body.appendChild(container);
	}
	var textColorClass = (type === 'warning') ? 'text-dark' : 'text-white';
	var closeClass = (type === 'warning') ? 'btn-close' : 'btn-close btn-close-white';
	var toastHtml = ''
		+ '<div class="toast align-items-center ' + textColorClass + ' bg-' + type + ' border-0" role="alert" aria-live="assertive" aria-atomic="true">'
		+ '<div class="d-flex">'
		+ '<div class="toast-body">' + message + '</div>'
		+ '<button type="button" class="' + closeClass + ' me-2 m-auto" data-bs-dismiss="toast" aria-label="Close"></button>'
		+ '</div>'
		+ '</div>';
	container.insertAdjacentHTML('beforeend', toastHtml);
	var toastElement = container.lastElementChild;
	var toast = new bootstrap.Toast(toastElement, { autohide: true, delay: 5000 });
	toast.show();
	toastElement.addEventListener('hidden.bs.toast', function () { this.remove(); });
}
